#include "serial.h"
#include "metrics.h"
#include "crc.h"
#include "../teensy_rc_control/Packets.h"

#include <termios.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


static int serport = -1;
static pthread_t serthread;
static bool serialRunning;
static uint64_t lastSerSendTime;
static uint8_t my_frame_id = 0;
static uint8_t peer_frameid = 0;

/* microseconds */
#define SER_SEND_INTERVAL 16384

static char const *charstr(int i, char *str) {
    if (i >= 32 && i < 127) {
        str[0] = i;
        str[1] = 0;
        return str;
    }
    str[0] = '.';
    str[1] = 0;
    return str;
}


static void hexdump(void const *vp, int end) {
    unsigned char const *p = (unsigned char const *)vp;
    int offset = 0;
    char s[10];
    while (offset < end) {
        fprintf(stderr, "%04x: ", offset);
        for (int i = 0; i != 16; ++i) {
            if (i+offset < end) {
                fprintf(stderr, " %02x", p[i+offset]);
            } else {
                fprintf(stderr, "   ");
            }
        }
        fprintf(stderr, " ");
        for (int i = 0; i != 16; ++i) {
            if (i+offset < end) {
                fprintf(stderr, "%s", charstr(p[i+offset], s));
            } else {
                fprintf(stderr, " ");
            }
        }
        fprintf(stderr, "\n");
        offset += 16;
    }
}

template<typename T> struct IncomingPacket {
    T data;
    bool fresh;
    uint64_t when;
};

static IncomingPacket<TrimInfo> g_TrimInfo;
static IncomingPacket<IBusPacket> g_IBusPacket;
static IncomingPacket<SteerControl> g_SteerControl;

TrimInfo const *serial_trim_info(uint64_t *oTime, bool *oFresh) {
    *oFresh = g_TrimInfo.fresh;
    if (g_TrimInfo.fresh) g_TrimInfo.fresh = false;
    *oTime = g_TrimInfo.when;
    return &g_TrimInfo.data;
}

IBusPacket const *serial_ibus_packet(uint64_t *oTime, bool *oFresh) {
    *oFresh = g_IBusPacket.fresh;
    if (g_IBusPacket.fresh) g_IBusPacket.fresh = false;
    *oTime = g_IBusPacket.when;
    return &g_IBusPacket.data;
}

SteerControl const *serial_steer_control(uint64_t *oTime, bool *oFresh) {
    *oFresh = g_SteerControl.fresh;
    if (g_SteerControl.fresh) g_SteerControl.fresh = false;
    *oTime = g_SteerControl.when;
    return &g_SteerControl.data;
}


#define RECVPACKET(type) \
    case type::PacketCode: \
        if (!unpack(g_##type, sizeof(type), ++buf, end)) { \
            fprintf(stderr, "Packet too short for %s: %ld\n", #type, (long)(end-buf)); \
            hexdump(buf, end-buf); \
            Serial_UnknownMessage.set(); \
            return; \
        } \
        break;

template<typename T> bool unpack(IncomingPacket<T> &dst, size_t dsz, unsigned char const *&src, unsigned char const *end) {
    if (end-src < (long)dsz) {
        return false;
    }
    memcpy(&dst.data, src, dsz);
    src += dsz;
    dst.fresh = true;
    dst.when = metric::Collector::clock();
    return true;
}

// buf points at frameid
static void handle_packet(unsigned char const *buf, unsigned char const *end) {
    assert(end - buf >= 5);
    uint16_t crc = crc_kermit(buf+2, end-buf-4);
    if (((crc & 0xff) != end[-2]) || (((crc >> 8) & 0xff) != end[-1])) {
        Serial_CrcErrors.increment();
        fprintf(stderr, "serial CRC got %x calculated %x\n", end[-2] | (end[-1] << 8), crc);
        return;
    }
    peer_frameid = buf[0];
    //  ignore lastseen for now
    //  ignore length; already handled by caller
    buf += 3;
    end -= 2;
    while (buf != end) {
        //  decode a packet
        switch (*buf) {
            RECVPACKET(TrimInfo);
            RECVPACKET(IBusPacket);
            RECVPACKET(SteerControl);
            default:
                fprintf(stderr, "serial unknown message ID 0x%02x; flushing to end of packet\n", *buf);
                hexdump(buf, end-buf);
                Serial_UnknownMessage.set();
                return;
        }
        Serial_PacketsDecoded.increment();
    }
}

/* 0x55 0xAA <frameid> <lastseen> <length> <payload> <CRC16-L> <CRC16-H>
 * CRC16 is CRC16 of <length> and <payload>.
 */
static void parse_buffer(unsigned char *buf, int &bufptr) {
again:
    if (bufptr < 7) {
        return; //  can't be anything
    }
    int i = 0;
    for (i = 0; i != bufptr; ++i) {
        if (buf[i] == 0x55 && buf[i+1] == 0xAA) {
            //  header
            if (bufptr < i + 7) {
                goto clear_to_i;
            }
            if (buf[i + 4] > 256-7) {
                //  must flush this packet
                i += 2;
                goto clear_to_i;
            }
            if (buf[i+4] + 7 < bufptr) {
                //  don't yet have all the data
                goto clear_to_i;
            }
            handle_packet(buf + i + 2, buf + 7 + buf[i+4]);
            i += buf[i+4] + 7;
            goto clear_to_i;
        }
    }
clear_to_i:
    if (i > 0) {
        if (i < bufptr) {
            memmove(buf, &buf[i], bufptr-i);
        }
        bufptr -= i;
        if (bufptr > 0) {
            goto again;
        }
    }
}

static bool send_outgoing_packet() {
    ++my_frame_id;
    if (!my_frame_id) {
        my_frame_id = 1;
    }
    unsigned char pack[256] = { 0x55, 0xAA, my_frame_id, peer_frameid, 0 };
    int dlen = 0, dmaxlen = 256-7;
    // pack in some stuff
    (void)&dmaxlen,(void)&dlen;
    pack[4] = (unsigned char)(dlen & 0xff);
    uint16_t crc = crc_kermit(&pack[4], dlen+1);
    pack[5+dlen] = (unsigned char)(crc & 0xff);
    pack[6+dlen] = (unsigned char)((crc >> 8) & 0xff);
    int wr = ::write(serport, pack, dlen+7);
    if (wr != dlen+7) {
        if (wr < 0) {
            perror("serial write");
        } else {
            fprintf(stderr, "serial short write: %d instead of %d\n", wr, dlen+7);
        }
        return false;
    }
    Serial_BytesSent.increment(wr);
    return true;
}

static void *ser_fn(void *) {
    int bufptr = 0;
    unsigned char inbuf[256] = { 0 };
    int nerror = 0;
    while (serialRunning) {
        if (bufptr == 256) {
            bufptr = 0; //  flush
        }
        uint64_t now = metric::Collector::clock();
        if (now - lastSerSendTime > SER_SEND_INTERVAL) {
            //  quantize to the send interval
            lastSerSendTime = now - (now % SER_SEND_INTERVAL);
            //  do send
            if (!send_outgoing_packet()) {
                Serial_Error.set();
                ++nerror;
                if (nerror >= 10) {
                    break;
                }
            }
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serport, &fds);
        struct timeval tv = { 0, 0 };
        tv.tv_usec = SER_SEND_INTERVAL - (now - lastSerSendTime);
        int n = select(serport+1, &fds, NULL, NULL, &tv);
        if (n > 0) {
            if (FD_ISSET(serport, &fds)) {
                int r = read(serport, &inbuf[bufptr], 256-bufptr);
                if (r < 0) {
                    if (errno != EAGAIN) {
                        if (serialRunning) {
                            Serial_Error.set();
                            perror("serial");
                            ++nerror;
                            if (nerror >= 10) {
                                break;
                            }
                        }
                    }
                    continue;
                }
                if (r > 0) {
                    Serial_BytesReceived.increment(r);
                    if (nerror) {
                        --nerror;
                    }
                    bufptr += r;
                }
                if (bufptr > 0) {
                    //  parse and perhaps remove data
                    parse_buffer(inbuf, bufptr);
                }
            }
        }
    }
    return 0;
}

#define SPEED(x) case x: return B ## x

speed_t constant_from_speed(int speed) {
    switch (speed) {
        SPEED(300);
        SPEED(1200);
        SPEED(2400);
        SPEED(9600);
        SPEED(19200);
        SPEED(38400);
        SPEED(57600);
        SPEED(115200);
        SPEED(230400);
        SPEED(460800);
        SPEED(921600);
        SPEED(1000000);
        SPEED(2000000);
        default:
            return B9600;
    }
}

bool start_serial(char const *port, int speed) {
    if (serthread) {
        return false;
    }
    if (serport != -1) {
        return false;
    }
    serport = open(port, O_RDWR);
    if (serport < 0) {
        perror(port);
        return false;
    }

    termios attr;
    tcgetattr(serport, &attr);
    cfmakeraw(&attr);
    cfsetspeed(&attr, constant_from_speed(speed));
    attr.c_cc[VMIN] = 0;
    attr.c_cc[VTIME] = 1;
    tcsetattr(serport, TCSAFLUSH, &attr);

    serialRunning = true;
    if (pthread_create(&serthread, NULL, ser_fn, NULL) < 0) {
        close(serport);
        serport = -1;
        serialRunning = false;
        return false;
    }
    return true;
}

void stop_serial() {
    if (serthread) {
        serialRunning = false;
        close(serport);
        void *x = 0;
        pthread_join(serthread, &x);
        serport = -1;
        serthread = 0;
    }
}


