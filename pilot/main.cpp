#include "glesgui.h"
#include "RaspiCapture.h"
#include "image.h"
#include "queue.h"
#include "network.h"
#include "metrics.h"
#include "settings.h"
#include "../stb/stb_image_write.h"
#include "serial.h"
#include "../teensy_rc_control/Packets.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>


static char errbuf[256];
static FrameQueue guiFromNetwork(3, 0, 0, 0, 0);
static Texture netTextureY;
static Texture netTextureU;
static Mesh netMeshY;
static Mesh netMeshU;
static MeshDrawOp drawMeshY;
static MeshDrawOp drawMeshU;
static int im_width;
static int im_height;
static int im_planes;
static size_t im_size;
static bool ibusWantsRecord;
static bool drawFlags = true;
static bool drawIbus = false;
static bool drawSteer = false;
static uint16_t ibusData[10];
static SteerControl steerControlData;


void do_click(int mx, int my, int btn, int st) {

}

void do_move(int x, int y, unsigned int btns) {

}

static size_t framesDrawn;
static uint64_t framesStart;
static uint64_t lastLoop;
static double meanFps = 15.0f;
static double fastFps = 15.0f;

void do_draw() {
    uint64_t now = metric::Collector::clock();
    gg_draw_mesh(&drawMeshY);
    gg_draw_mesh(&drawMeshU);
    char fpsText[20];
    fastFps = fastFps * 0.9 + 1e6 / (now - lastLoop) * 0.1;
    sprintf(fpsText, "%4.1f %4.1f", meanFps, fastFps);
    gg_draw_text(3, 3, 0.75f, fpsText, color::textred);

    if (drawFlags) {
        int y = 2;
        for (metric::Flag const *f = metric::Collector::iterFlags(); f; f = metric::Collector::nextFlag(f)) {
            bool flag = false;
            double avg = 0.0;
            uint64_t tim = 0;
            f->get(flag, avg, tim);
            uint32_t c = flag ? color::textyellow : color::textblue;
            gg_draw_text(3, y * 20, 0.75f, f->name(), c);
            gg_draw_text(400, y * 20, 0.75f, flag ? "ON" : "", c);
            sprintf(fpsText, "%4.2f", avg);
            gg_draw_text(450, y * 20, 0.75f, fpsText, c);
            double age = (now - tim) * 1e-6;
            if (age < 100.0f) {
                sprintf(fpsText, "%5.2f", age);
            } else {
                strcpy(fpsText, "old");
            }
            gg_draw_text(520, y * 20, 0.75f, fpsText, c);
            y += 1;
        }
    }

    if (drawSteer) {
        gg_draw_box(400-328, 300, 400+328, 340, color::bggray);
        if (steerControlData.steer == (int16_t)0x8000) {
            gg_draw_box(360, 300, 440, 340, color::bgred);
        } else if (abs(steerControlData.steer) < 10) {
            gg_draw_box(398, 300, 402, 340, color::bggreen);
        } else {
            gg_draw_box(400 + steerControlData.steer / 50, 300, 400, 340, color::bgblue);
        }
    }

    if (drawIbus) {
        for (int i = 0; i != 10; ++i) {
            char buf[12];
            sprintf(buf, "%4d", ibusData[i]); 
            gg_draw_text(10+70*i, 300, 0.75f, buf, color::textgray);
        }
    }

    if (framesDrawn == 64) {
        double newFps = framesDrawn * 1e6 / (now - framesStart);
        Graphics_Fps.sample(newFps);
        framesDrawn = 0;
        framesStart = now;
        meanFps = meanFps * 0.8 + newFps * 0.2;
    }
    ++framesDrawn;
    lastLoop = now;
}

static unsigned char byte_from_float(float f) {
    if (f < 0) return 0;
    if (f > 1) return 255;
    return (unsigned char)(f * 255);
}

void update_comms() {
    uint64_t now = get_microseconds();
    bool fresh = false;
    uint64_t time = 0;
    SteerControl const *steerControl = serial_steer_control(&time, &fresh);
    if (steerControl && fresh) {
        Packet_SteerControlCount.increment();
        //  push into recording stream
        insert_metadata(SteerControl::PacketCode, steerControl, sizeof(*steerControl));
        memcpy(&steerControlData, steerControl, sizeof(steerControlData));
    }
    Packet_SteerControlAge.sample((now - time) * 1e-6);

    IBusPacket const *ibusPacket = serial_ibus_packet(&time, &fresh);
    if (ibusPacket && fresh) {
        Packet_IBusPacketCount.increment();
        ibusWantsRecord = ibusPacket->data[7] > 1700;
        //  push into recording stream
        insert_metadata(IBusPacket::PacketCode, ibusPacket, sizeof(*ibusPacket));
        memcpy(ibusData, ibusPacket->data, sizeof(ibusData));
    }
    double ibusAge = (now - time) * 1e-6;
    Packet_IBusPacketAge.sample(ibusAge);
    if (ibusAge > 1.5) {
        ibusWantsRecord = false;
    }

    TrimInfo const *trimInfo = serial_trim_info(&time, &fresh);
    if (trimInfo && fresh) {
        Packet_TrimInfoCount.increment();
        //  push into recording stream
        insert_metadata(TrimInfo::PacketCode, trimInfo, sizeof(*trimInfo));
    }
    Packet_TrimInfoAge.sample((now - time) * 1e-6);

    /* derive recording state from possibly multiple sources */
    bool shouldRecord = false;
    if (ibusWantsRecord) {
        shouldRecord = true;
    }
    set_recording(shouldRecord);
}

void do_idle() {
    Frame *fr = NULL;
    while (Frame *f2 = guiFromNetwork.beginRead()) {
        if (fr) {
            fr->endRead();
        }
        fr = f2;
    }
    if (fr) {
        float const *f = (float const *)fr->link_->data_;
        unsigned char *t = (unsigned char *)netTextureY.mipdata[0];
        for (int r = 0; r != im_height; ++r) {
            t = (unsigned char *)netTextureY.mipdata[0] + netTextureY.width * r;
            for (int c = 0; c != im_width; ++c) {
                *t++ = byte_from_float(*f++);
            }
        }
        for (int r = 0; r != im_height; ++r) {
            t = (unsigned char *)netTextureU.mipdata[0] + netTextureU.width * r;
            for (int c = 0; c != im_width; ++c) {
                *t++ = byte_from_float(*f++);
            }
        }
        fr->endRead();
        gg_update_texture(&netTextureY, 0, im_width, 0, im_height);
        gg_update_texture(&netTextureU, 0, im_width, 0, im_height);
    }

}


int nframes = 0;
uint64_t start;
static FrameQueue *networkQueue;
static int numMissed;
static int numProcessed;

void buffer_callback(void *data, size_t size, void *cookie) {
    Frame *f = networkQueue->beginWrite();
    if (f == NULL) {
        ++numMissed;
    } else {
        unwarp_image(data, f->data_);
        f->endWrite();
        ++numProcessed;
        Process_WarpBuffers.increment();
    }
    ++nframes;
    Process_TotalBuffers.increment();
    if (!(nframes & 0x3f)) {
        uint64_t stop = metric::Collector::clock();
        Process_WarpFps.sample((0x40)/((stop-start)*1e-6));
        Process_WarpPercent.sample(float(numProcessed) * 100.0f / (numProcessed+numMissed));
        start = stop;
        numProcessed = 0;
        numMissed = 0;
    }
}

float guiPictureVbufY[] = {
68,470,0,0,
366,470,1,0,
366,352,1,1,
68,352,0,1,
};

float guiPictureVbufU[] = {
434,470,0,0,
732,470,1,0,
732,352,1,1,
434,352,0,1,
};

unsigned short guiPictureIbuf[] = {
0,1,2,
2,3,0,
};

bool build_gui() {
    gg_allocate_texture(NULL, im_width, im_height, 1, 1, &netTextureY);
    gg_allocate_texture(NULL, im_width, im_height, 1, 1, &netTextureU);
    float su = float(im_width) / netTextureY.width;
    float sv = float(im_height) / netTextureU.height;
    for (int i = 0; i != 4; ++i) {
        guiPictureVbufY[4*i+2] *= su;
        guiPictureVbufY[4*i+3] *= sv;
        guiPictureVbufU[4*i+2] *= su;
        guiPictureVbufU[4*i+3] *= sv;
    }
    gg_allocate_mesh(guiPictureVbufY, 16, 16, guiPictureIbuf, 6, 8, 0, &netMeshY, 0);
    gg_allocate_mesh(guiPictureVbufU, 16, 16, guiPictureIbuf, 6, 8, 0, &netMeshU, 0);
    drawMeshY.program = gg_compile_named_program("gui-texture", errbuf, sizeof(errbuf));
    drawMeshY.texture = &netTextureY;
    drawMeshY.mesh = &netMeshY;
    drawMeshY.transform = gg_gui_transform();
    gg_init_color(drawMeshY.color, 0, 1, 1, 1);
    drawMeshU = drawMeshY;
    drawMeshU.texture = &netTextureU;
    drawMeshU.mesh = &netMeshU;
    gg_init_color(drawMeshU.color, 1, 1, 0, 1);
    return true;
}

static volatile bool commsRunning;
static pthread_t commsThread;

void *comms_thread(void *) {
    while (commsRunning) {
        usleep(50000);
        update_comms();
    }
    return NULL;
}

void setup_run() {

    char const *network = get_setting("network", "network");
    if (!load_network(network, &guiFromNetwork)) {
        fprintf(stderr, "network %s could not be loaded\n", network);
        exit(1);
    }
    fprintf(stderr, "loaded network '%s'\n", network);

    networkQueue = network_input_queue();

    get_unwarp_info(&im_size, &im_width, &im_height, &im_planes);
    build_gui();

    static char path[1024];
    time_t t;
    time(&t);
    char const *capture = get_setting("capture", "/var/tmp/pilot/capture");
    sprintf(path, "%s-%ld", capture, (long)t);
    CaptureParams cap = {
        640, 480, path, &buffer_callback, NULL
    };
    network_start();
    setup_capture(&cap);
    fprintf(stderr, "capture directed to %s\n", capture);

    char const *tty = get_setting("serial", "/dev/ttyACM0");
    int speed = get_setting_int("serialbaud", 1000000);
    if (!start_serial(tty, speed)) {
        fprintf(stderr, "Could not open serial port %s: %s\n", tty, strerror(errno));
        exit(1);
    }

    commsRunning = true;
    pthread_create(&commsThread, NULL, comms_thread, NULL);
    //set_recording(true);
}

void enum_errors(char const *err, void *p) {
    (*(int *)p) += 1;
    fprintf(stderr, "%s\n", err);
}


bool configure_metrics() {
    char const *fn = get_setting("metrics-out", "/var/tmp/pilot/metrics");
    char filename[1024];
    time_t t;
    time(&t);
    sprintf(filename, "%s-%ld.csv", fn, (long)t);
    if (!start_metrics(filename)) {
        return false;
    }
    fprintf(stderr, "logging metrics to %s\n", filename);
    return true;
}

int main(int argc, char const *argv[]) {
    mkdir("/var/tmp/pilot", 0775);
    if (load_settings("pilot")) {
        fprintf(stderr, "Loaded settings from '%s'\n", "pilot");
    }
    if (gg_setup(800, 480, 0, 0, "pilot") < 0) {
        fprintf(stderr, "GLES context setup failed\n");
        return -1;
    }
    int compileErrors = 0;
    bool compiled = false;
    while (argv[1] && argv[2] && !strcmp(argv[1], "--compile")) {
        compiled = true;
        Program const *p = gg_compile_named_program(argv[2], errbuf, sizeof(errbuf));
        if (!p) {
            fprintf(stderr, "Failed to compile %s program:\n%s\n", argv[2], errbuf);
            ++compileErrors;
        }
        argv += 2;
        argc -= 2;
    }
    while (argv[1] && argv[2] && !strcmp(argv[1], "--loadmesh")) {
        compiled = true;
        Mesh const *p = gg_load_named_mesh(argv[2], errbuf, sizeof(errbuf));
        if (!p) {
            fprintf(stderr, "Failed to load %s mesh:\n%s\n", argv[2], errbuf);
            ++compileErrors;
        }
        argv += 2;
        argc -= 2;
    }
    while (argv[1] && argv[2] && !strcmp(argv[1], "--loadtexture")) {
        compiled = true;
        Texture const *p = gg_load_named_texture(argv[2], errbuf, sizeof(errbuf));
        if (!p) {
            fprintf(stderr, "Failed to load %s texture:\n%s\n", argv[2], errbuf);
            ++compileErrors;
        }
        argv += 2;
        argc -= 2;
    }
    if (compiled) {
        if (compileErrors) {
            return -1;
        }
        return 0;
    }
    if (argc != 2 || strcmp(argv[1], "--test")) {
        gg_onmousebutton(do_click);
        gg_onmousemove(do_move);
        gg_ondraw(do_draw);
        if (!configure_metrics()) {
            fprintf(stderr, "Can't run without metrics\n");
            return -1;
        }
        setup_run();
        gg_run(do_idle);
    }
    commsRunning = false;
    stop_serial();
    network_stop();
    stop_metrics();
    int num = 0;
    gg_get_gl_errors(enum_errors, &num);
    return (num > 0) ? 1 : 0;
}

