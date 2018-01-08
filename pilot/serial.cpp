#include "serial.h"
#include "metrics.h"
#include "crc.h"
#include "settings.h"
#include "Packets.h"

#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <linux/input.h>
#include <linux/hidraw.h>


float gSteerScale = 1.0f;
float gThrottleScale = 0.4f;
float gThrottleMin = 0.06f;
float gSteer = 0.0f;
float gThrottle = 0.0f;
float gTweakSteer = 0.0;

static int serport;
static bool serialRunning = true;
static int64_t lastSerialSendTime;
static pthread_t serialThread;
static pthread_mutex_t serialMutex = PTHREAD_MUTEX_INITIALIZER;



/* microseconds */
#define SERIAL_SEND_INTERVAL 19000

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


static uint16_t map_rc(float f) {
    return (f < -1.0f) ? 1000 : (f > 1.0f) ? 2000 : (uint16_t)(1500 + f * 500);
}

static float clamp(float val, float from, float to) {
    if (val < from) return from;
    if (val > to) return to;
    return val;
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
static IncomingPacket<VoltageLevels> g_VoltageLevels;

static IncomingPacket<SteerControl> g_SteerControl;
static IncomingPacket<EchoEnabled> g_EchoEnabled;
static IncomingPacket<TurnOff> g_TurnOff;



void serial_steer(float steer, float throttle) {
    pthread_mutex_lock(&serialMutex);
    gSteer = clamp(steer * gSteerScale, -1.0f, 1.0f);
    gThrottle = gThrottleMin + clamp(throttle * gThrottleScale, 0, 1.0f);
    pthread_mutex_unlock(&serialMutex);
}

void serial_power_off() {
    pthread_mutex_lock(&serialMutex);
    g_TurnOff.fresh = true;
    g_TurnOff.when = metric::Collector::clock();
    pthread_mutex_unlock(&serialMutex);
}

void serial_enable_echo(bool en) {
    pthread_mutex_lock(&serialMutex);
    g_EchoEnabled.echo = en ? 1 : 0;
    g_EchoEnabled.fresh = true;
    g_EchoEnabled.when = metric::Collector::clock();
    pthread_mutex_unlock(&serialMutex);
}

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

IBusPacket const *serial_voltage_levels(uint64_t *oTime, bool *oFresh) {
    *oFresh = g_VoltageLevels.fresh;
    if (g_VoltageLevels.fresh) g_VoltageLevels.fresh = false;
    *oTime = g_VoltageLevels.when;
    return &g_VoltageLevels.data;
}



#define RECVPACKET(type) \
    case type::Token: \
        if (!unpack(g_##type, sizeof(type), ++buf, end)) { \
            fprintf(stderr, "Packet too short for %s: %ld\n", #type, (long)(end-buf)); \
            hexdump(buf, end-buf); \
            Serial_UnknownMessage.set(); \
            return; \
        } \
        break;

template<typename T> bool unpack(IncomingPacket<T> &dst, size_t dsz, unsigned char const *&src, unsigned char const *end) {
    unsigned char const *ss = src;
    int num = 0;
    int32_t *ip = reinterpret_cast<int32_t *>(&dst);
    while (*ss != '\r' && *s != '\n') {
        while (*ss == ' ') {
            ++ss;
        }
        char *oot = nullptr;
        long l = strtol((char const*)ss, &oot, 10);
        if (oot && oot >= (char const *)ss) {
            *ip++ = (int32_t)l;;
            ss = (unsigned char const *)oot;
        } else {
            //  short receive
            return false;
        }
    }
    src = ss;
    dst.fresh = true;
    dst.when = metric::Collector::clock();
    return true;
}

// buf points at frameid
static void handle_packet(unsigned char const *buf, unsigned char const *end) {
    unsigned char code = *buf;
    ++buf;
    //  decode a packet
    switch (*buf) {
        RECVPACKET(TrimInfo);
        RECVPACKET(IBusPacket);
        RECVPACKET(VoltageLevels);
        default:
            fprintf(stderr, "serial unknown message ID 0x%02x; flushing to end of packet\n", *buf);
            hexdump(buf, end-buf);
            Serial_UnknownMessage.set();
            return;
    }
    Serial_PacketsDecoded.increment();
}

template<typename T> char *encode(T &t, char *dst, char *end) {
    if (!dst) return nullptr;
    if (end-dst < 4) return nullptr;
    *dst++ = T::Token);
    int32_t *p = reinterpret_cast<int32_t *>(&t);
    for (int i = 0; i != T::NumInts; ++i) {
        int q = snprintf(dst, end-dst, " %d", p[i]);
        if (q >= end-dst) {
            return nullptr;
        }
        dst += q;
    }
    if (end-dst < 3) {
        return nullptr;
    }
    strcpy(dst, "\r\n");
    return dst+3;
}

static bool send_outgoing_packet() {
    char obuf[1025], *ptr = obuf, *end = &obuf[1024];
    pthread_mutex_lock(&serialMutex);
    if (TurnOff.fresh) {
        ptr = encode(g_TurnOff.data, ptr, end);
        TurnOff.fresh = false;
    }
    if (EchoEnabled.fresh) {
        ptr = encode(g_EchoEnabled.data, ptr, end);
        EchoEnabled.fresh = false;
    }
    if (SteerControl.when > metric::Collector::clock()-1000000) {
        ptr = encode(g_SteerControl.data, ptr, end);
    }
    pthread_mutex_unlock(&serialMutex);
    if (!ptr) {
        fprintf(stderr, "serial marshalling error\n");
        return false;
    }
    if (ptr != obuf) {
        int wr = ::write(hidport, obuf, ptr-obuf);
        if (wr != ptr-obuf) {
            if (wr < 0) {
                perror("serial write");
            } else {
                fprintf(stderr, "serial short write: %d instead of %d\n", wr, dlen+7);
            }
            return false;
        }
        Serial_BytesSent.increment(ptr-obuf);
    }
    return true;
}

static void *ser_fn(void *) {
    int bufptr = 0;
    unsigned char inbuf[256] = { 0 };
    int nerror = 0;
    puts("start serial thread");
    bool shouldsleep = false;
    serial_enable_echo(true);
    while (serialRunning) {
        if (bufptr == 256) {
            bufptr = 0; //  flush
        }
        uint64_t now = metric::Collector::clock();
        int n = 0;
        if (shouldsleep) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(serport, &fds);
            struct timeval tv = { 0, 0 };
            tv.tv_usec = HID_SEND_INTERVAL - (now - lastSerialSendTime);
            if (tv.tv_usec < 1) {
                tv.tv_usec = 1;
            }
            if (tv.tv_usec > 20000) {
                tv.tv_usec = 20000;
            }
            n = select(serport+1, &fds, NULL, NULL, &tv);
            now = metric::Collector::clock();
        }
        if (n >= 0) {
            if (now - lastSerialSendTime >= SERIAL_SEND_INTERVAL) {
                //  quantize to the send interval
                lastSerialSendTime = now - (now % SERIAL_SEND_INTERVAL);
                //  do send
                if (!send_outgoing_packet()) {
                    fprintf(stderr, "\nsend error");
                    Serial_Error.set();
                    ++nerror;
                    if (nerror >= 10) {
                        break;
                    }
                }
            }
            int r = read(serport, &inbuf[bufptr], sizeof(inbuf)-bufptr);
            if (r < 0) {
                shouldsleep = true;
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
            } else {
                shouldsleep = false;
                if (r > 0) {
                    Serial_BytesReceived.increment(r);
                    if (nerror) {
                        --nerror;
                    }
                    bufptr += r;
                }
                if (bufptr > 1) {
                    for (int i = 1; i != bufptr; ++i) {
                        if (inbuf[i] == '\n' && inbuf[i-1] == '\r') {
                            parse_buffer(inbuf, i);
                            if (i != bufptr) {
                                memmove(inbuf, &inbuf[i], bufptr-i);
                            }
                            bufptr -= i;
                            i = -1;
                        }
                    }
                }
            }
        } else {
            perror("select");
            ++nerror;
            if (nerror >= 10) {
                break;
            }
        }
    }
    //  turn off echo
    serial_enable_echo(false);      //  track state
    write(serport, "X 0\r\n", 5);   //  actuall tell it
    puts("end serial thread");
    return 0;
}

#define TOKEN(x) \
    case x: return B##x

static int speed_token(int speed) {
    switch (speed) {
        TOKEN(9600);
        TOKEN(19200);
        TOKEN(38400);
        TOKEN(57600);
        TOKEN(115200);
        TOKEN(230400);
        TOKEN(1000000);
    }
    return B115200;
}

static int verify_open(char const *name, int speed) {
    fd = open(name, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }
    struct termios tio;
    int r = tcgetattr(fd, &tio);
    if (r < 0) {
        perror("tcgetattr");
    }
    cfsetspeed(&tio, speed_token(speed));
    cfmakeraw(&tio);
    tio.c_iflag &= ~(IXON | IXOFF | OFILL);
    r = tcsetattr(fd, &tio, TCSAFLUSH);
    if (r < 0) {
        perror("tcsetattr");
    }
    fprintf(stderr, "serial open %s returns fd %d\n", name, fd);
    return fd;
}

bool open_serport(char const *port, int speed) {
    serport = verify_open(port, speed);
    if (serport < 0) {
        serport = verify_open("/dev/ttyACM0", speed);
        if (serport < 0) {
            fprintf(stderr, "%s is not a recognized serial device, and couldn't find one at /dev/ttyACM0.\n", port);
            return false;
        }
    }
    return true;
}

bool start_serial(char const *port, int speed) {
    if (serialThread) {
        return false;
    }
    if (serport != -1) {
        return false;
    }
    gTweakSteer = get_setting_float("steer_tweak", gTweakSteer);
    gSteerScale = get_setting_float("steer_scale", gSteerScale);
    gThrottleScale = get_setting_float("throttle_scale", gThrottleScale);
    gThrottleMin = get_setting_float("throttle_min", gThrottleMin);

    open_serport(port, speed);
    if (serport < 0) {
        return false;
    }
    serialRunning = true;
    atexit(stop_serial);
    if (pthread_create(&serialThread, NULL, ser_fn, NULL) < 0) {
        close(serport);
        serport = -1;
        serialRunning = false;
        return false;
    }
    return true;
}

void stop_serial() {
    if (serialThread) {
        serialRunning = false;
        void *x = 0;
        pthread_join(serialThread, &x);
        close(serport);
        serport = -1;
        serialThread = 0;
    }
}


