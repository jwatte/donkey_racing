#include "metrics.h"
#include "plock.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>


using namespace metric;

Counter Graphics_GlError("graphics.glerror");
Flag Graphics_AlwaysMiplevel0("graphics.alwaysmiplevel0");
Flag Graphics_AlwaysTeximage("graphics.alwaysteximage");
Flag Graphics_AlwaysRgba("graphics.alwaysrgba");
Sampler Graphics_Fps("graphics.fps");
Counter Process_TotalBuffers("process.warpbuffers");
Counter Process_WarpBuffers("process.warpbuffers");
Counter Process_NetBuffers("process.netbuffers");
Sampler Process_WarpFps("process.warpfps");
Sampler Process_WarpPercent("process.warppercent");
Sampler Process_UnwarpTime("process.unwarptime");
Sampler Process_NetFps("process.netfps");
Sampler Process_NetDuration("process.netduration");
Counter Encoder_BufferCount("encoder.buffercount");
Sampler Encoder_BufferSize("encoder.buffersize");
Counter Capture_BufferCount("capture.buffercount");
Sampler Capture_BufferSize("capture.buffersize");
Flag Capture_Recording("capture.recording");
Flag Serial_Error("serial.error");
Counter Serial_BytesReceived("serial.bytesreceived");
Counter Serial_BytesSent("serial.bytessent");
Counter Serial_CrcErrors("serial.crcerrors");
Flag Serial_UnknownMessage("serial.unknownmessage");
Counter Serial_PacketsDecoded("serial.packetsdecoded");
Sampler Packet_SteerControlAge("packet.steercontrolage");
Counter Packet_SteerControlCount("packet.steercontrolcount");
Sampler Packet_IBusPacketAge("packet.ibuspacketage");
Counter Packet_IBusPacketCount("packet.ibuspacketcount");
Sampler Packet_TrimInfoAge("packet.triminfoage");
Counter Packet_TrimInfoCount("packet.triminfocount");

static volatile bool collectRunning = false;
static pthread_t collectThread;

typedef long long unsigned int llu;

static uint64_t dumpTime;
static void dump_counters(int type, char const *name, MetricBase const *mb, void *cookie) {
    switch (type) {
        default:
        case 0:
            fprintf((FILE *)cookie, "%llu,%d,%s,%llu,%llu,%llu,%.5e\n",
                    (llu)dumpTime, type, name,
                    (llu)mb->time_, (llu)mb->count_,
                    (llu)mb->value_.i, mb->sum_);
            break;
        case 1:
            fprintf((FILE *)cookie, "%llu,%d,%s,%llu,%llu,%.5e,%.5e\n",
                    (llu)dumpTime, type, name,
                    (llu)mb->time_, (llu)mb->count_,
                    mb->value_.d, mb->sum_);
            break;
        case 2:
            fprintf((FILE *)cookie, "%llu,%d,%s,%llu,%llu,%d,%.5e\n",
                    (llu)dumpTime, type, name,
                    (llu)mb->time_, (llu)mb->count_, mb->value_.b ? 1 : 0, mb->sum_);
            break;
    }
}

static void *collect_fn(void *ofile) {
    uint64_t base = Collector::clock();
    while (collectRunning) {
        uint64_t now = Collector::clock();
        if (now - base >= 5000000) {
            base += 5000000;
            dumpTime = now;
            Collector::collect(dump_counters, ofile);
            fflush((FILE *)ofile);
            fdatasync(fileno((FILE *)ofile));
        }
        if (!collectRunning) {
            break;
        }
        usleep(100000);
    }
    fclose((FILE *)ofile);
    return 0;
}

bool start_metrics(char const *filename) {
    if (!collectThread) {
        collectRunning = true;
        FILE *ofile = fopen(filename, "wb");
        if (!ofile) {
            return false;
        }
        if (pthread_create(&collectThread, NULL, collect_fn, ofile) < 0) {
            collectRunning = false;
            collectThread = 0;
            fclose(ofile);
            return false;
        }
        return true;
    }
    return false;
}

void stop_metrics() {
    if (collectThread) {
        collectRunning = false;
        void *x;
        pthread_join(collectThread, &x);
        collectThread = 0;
    }
}
