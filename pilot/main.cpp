#include "glesgui.h"
#include "truetype.h"
#include "RaspiCapture.h"
#include "image.h"
#include "queue.h"
#include "network.h"
#include "metrics.h"
#include "settings.h"
#include "../stb/stb_image_write.h"
#include "serial.h"
#include "Packets.h"
#include "lapwatch.h"
#include "filethread.h"
#include "cleanup.h"
#include <caffe2/core/logging_is_google_glog.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>


/* When this is set to non-zero, the camera pipeline
 * is started. This ends up doing "something" that 
 * causes the VCore to stall out and make GL run at 
 * just a handful of frames per second.
 */
#define VERY_SLOW_GL 1

extern float gSteer;
extern float gThrottle;


static char errbuf[256];
static FrameQueue guiFromNetwork(3, 0, 0, 0, 0);
static Texture netTextureY;
static Mesh netMeshY;
static MeshDrawOp drawMeshY;
static int im_width;
static int im_height;
static int im_planes;
static size_t im_size;
static bool ibusWantsRecord;
static bool drawFlags = true;
static bool drawIbus = true;
static bool drawSteer = true;
static uint16_t ibusData[10];
static SteerControl steerControlData;
static bool drawMetrics = true;

static int mousex = 0;
static int mousey = 0;
static bool displayingMenu = false;

static Mesh cursorMesh;
static MeshDrawOp cursorDraw;
static float cursorTransform[16];


struct MenuItem {
    float left;
    float width;
    float bottom;
    float height;
    void (*func)(MenuItem *i, int x, int y);
    char const *text;
};

void do_quit(MenuItem *, int x, int y) {
    fprintf(stderr, "Menu item: do_quit()\n");
    exit(0);
}

void do_steer_scale(MenuItem *mi, int x, int y) {
    gSteerScale = float(x) / mi->width + 0.1f;
    fprintf(stderr, "do_steer_scale(%d, %d) -> %.2f\n", x, y, gSteerScale);
    set_setting_float("steer_scale", gSteerScale);
}

void do_throttle_scale(MenuItem *mi, int x, int y) {
    gThrottleScale = 0.5f * float(x) / mi->width + 0.1f;
    fprintf(stderr, "do_throttle_scale(%d, %d) -> %.2f\n", x, y, gThrottleScale);
    set_setting_float("throttle_scale", gThrottleScale);
}

void do_toggle_metrics(MenuItem *mi, int x, int y) {
    drawMetrics = !drawMetrics;
}

void do_save_settings(MenuItem *, int, int) {
    fprintf(stderr, "save_settings()\n");
    save_settings("pilot");
}

MenuItem gMenu[] = {
    { 100, 600,  50, 60,           do_quit, "Quit"            },
    { 100, 600, 150, 60,    do_steer_scale, "Steer Scale"     },
    { 100, 600, 250, 60, do_throttle_scale, "Throttle Scale"  },
    { 100, 600, 350, 60, do_toggle_metrics, "Toggle Metrics"  },
    { 100, 600, 450, 60,  do_save_settings, "Save Settings"   },
    { 0 }
};

Mesh gMenuMesh;
MeshDrawOp gMenuDraw;
float menuXform[16];


void do_click(int mx, int my, int btn, int st) {
    fprintf(stderr, "click detected %d %d %d %d\n", 
            mx, my, btn, st);
    mousex = mx;
    mousey = my;
    if (st) {
        if (displayingMenu) {
            for (MenuItem *m = gMenu; m->func; ++m) {
                if (mx >= m->left && mx <= m->left + m->width && my >= m->bottom && my <= m->bottom + m->height) {
                    m->func(m, mx - m->left, my - m->bottom);
                    break;
                }
            }
            displayingMenu = false;
        } else if (btn == 0 && st && mx > 800 && my > 400) {
            //  top-right corner
            displayingMenu = true;
        }
    }
}


void do_move(int x, int y, unsigned int btns) {
    mousex = x;
    mousey = y;
}

void build_menu(Program const *p) {
    std::vector<TTVertex> vec;
    std::vector<unsigned short> ind;
    int n = 0;
    for (MenuItem *mi = gMenu; mi->func; ++mi) {
        vec.push_back({mi->left, mi->bottom, 0, 0});
        vec.push_back({mi->left+mi->width, mi->bottom, 1, 0});
        vec.push_back({mi->left+mi->width, mi->bottom+mi->height, 1, 1});
        vec.push_back({mi->left, mi->bottom+mi->height, 0, 1});
        ind.push_back(n);
        ind.push_back(n+1);
        ind.push_back(n+2);
        ind.push_back(n+2);
        ind.push_back(n+3);
        ind.push_back(n);
        n += 4;
    }
    gg_allocate_mesh(&vec[0], sizeof(TTVertex), vec.size(), &ind[0], ind.size(), 8, 0, &gMenuMesh, 0);
    gMenuDraw.program = p;
    char err[200];
    gMenuDraw.texture = gg_load_named_texture("menuitem", err, 200);
    gMenuDraw.mesh = &gMenuMesh;
    gg_get_gui_transform(menuXform);
    gMenuDraw.transform = menuXform;
    gg_init_color(gMenuDraw.color, 1, 1, 1, 1);
    gMenuDraw.primitive = PK_Triangles;
}

static void draw_menu() {
    if (displayingMenu) {
        gg_draw_mesh(&gMenuDraw);
        for (MenuItem *mi = gMenu; mi->func; ++mi) {
            gg_draw_text(mi->left+50, mi->bottom+10, 1.25f, mi->text, color::black);
        }
    }
}


static void draw_cursor() {
    gg_get_gui_transform(cursorTransform);
    cursorTransform[12] += mousex * cursorTransform[0];
    cursorTransform[13] += mousey * cursorTransform[5];
    gg_draw_mesh(&cursorDraw);
}

static size_t framesDrawn;
static uint64_t framesStart;
static uint64_t lastLoop;
static double meanFps = 15.0f;
static double fastFps = 15.0f;

void do_draw() {
    char fpsText[60] = { 0 };
    uint64_t now = metric::Collector::clock();
    gg_draw_mesh(&drawMeshY);
    START_WATCH("do_draw");

    if (!displayingMenu && drawMetrics && drawFlags) {
        int y = 2;
        for (metric::Flag const *f = metric::Collector::iterFlags(); f; f = metric::Collector::nextFlag(f)) {
            bool flag = false;
            double avg = 0.0;
            uint64_t tim = 0;
            f->get(flag, avg, tim);
            if (flag) {
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
    }
    LAP_WATCH("drawFlags");

    if (!displayingMenu && drawSteer) {
        gg_draw_box(512-328, 300, 512+328, 340, color::bggray);
        if (steerControlData.steer == (int16_t)0x8000) {
            gg_draw_box(512-40, 300, 512+40, 340, color::bgred);
        } else if (abs(steerControlData.steer) < 10) {
            gg_draw_box(512-2, 300, 512+2, 340, color::bggreen);
        } else {
            gg_draw_box(512 + steerControlData.steer / 50, 300, 512, 340, color::bgblue);
        }
    }
    LAP_WATCH("drawSteer");

    if (!displayingMenu && drawMetrics && drawIbus) {
        for (int i = 0; i != 10; ++i) {
            char buf[12];
            sprintf(buf, "%4d", ibusData[i]); 
            gg_draw_text(10+70*i, 270, 0.75f, buf, color::textgray);
        }
    }
    LAP_WATCH("drawIbus");

    draw_menu();
    LAP_WATCH("draw_menu");

    fastFps = fastFps * 0.8 + 1e6 / (now - lastLoop) * 0.2;
    sprintf(fpsText, "FPS: %5.1f %5.1f  Scl: %5.2f %5.2f  Ctl: %5.2f %5.2f",
            meanFps, fastFps, gSteerScale, gThrottleScale, gSteer, gThrottle);

    ++framesDrawn;
    if (now - framesStart >= 1000000) {
        fprintf(stderr, "\n%s\n", fpsText);
        meanFps = framesDrawn * 1e6 / (now - framesStart);
        Graphics_Fps.sample(meanFps);
        framesDrawn = 0;
        framesStart = now;
    }

    gg_draw_text(3, 3, 0.75f, fpsText, color::textred);
    LAP_WATCH("fps");

    draw_cursor();
    LAP_WATCH("draw_cursor");

    LAP_REPORT();
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
        ibusWantsRecord = ibusPacket->data[7] > 1300;
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
    //  Keep only the latest frame
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
        fr->endRead();
        gg_update_texture(&netTextureY, 0, im_width, 0, im_height);
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
        uint64_t w_start = metric::Collector::clock();
        unwarp_image(data, f->data_);
        f->endWrite();
        ++numProcessed;
        uint64_t w_end = metric::Collector::clock();
        Process_UnwarpTime.sample((w_end - w_start) * 1e-6);
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

unsigned short guiPictureIbuf[] = {
0,1,2,
2,3,0,
};

float guiCursorVbuf[] = {
    32, 0, 1, 0,
    0, 0, 0, 0,
    0, -32, 0, 1,
    32, -32, 1, 1,
};

bool build_gui() {
    gg_allocate_texture(NULL, im_width, im_height, 1, 1, &netTextureY);
    float su = float(im_width) / netTextureY.width;
    float sv = float(im_height) / netTextureY.height;
    for (int i = 0; i != 4; ++i) {
        guiPictureVbufY[4*i+2] *= su;
        guiPictureVbufY[4*i+3] *= sv;
    }
    gg_allocate_mesh(guiPictureVbufY, 16, 16, guiPictureIbuf, 6, 8, 0, &netMeshY, 0);
    drawMeshY.program = gg_compile_named_program("gui-texture", errbuf, sizeof(errbuf));
    drawMeshY.texture = &netTextureY;
    drawMeshY.mesh = &netMeshY;
    drawMeshY.transform = gg_gui_transform();
    gg_init_color(drawMeshY.color, 1, 1, 1, 1);

    gg_allocate_mesh(guiCursorVbuf, 16, 16, guiPictureIbuf, 6, 8, 0, &cursorMesh, 0);
    cursorDraw.program = drawMeshY.program;
    cursorDraw.texture = gg_load_named_texture("cursor", errbuf, sizeof(errbuf));
    cursorDraw.mesh = &cursorMesh;
    cursorDraw.transform = cursorTransform;
    gg_init_color(cursorDraw.color, 0.8, 0.5, 0.2, 1);

    build_menu(drawMeshY.program);

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

void do_int(int) {
    gg_set_quit_flag();
}


void setup_run() {

    char const *network = get_setting("network", "network");
    if (!load_network(network, &guiFromNetwork)) {
        fprintf(stderr, "network %s could not be loaded\n", network);
        exit(1);
    }
    fprintf(stderr, "loaded network '%s'\n", network);

    networkQueue = network_input_queue();

    start_filethread();
    get_unwarp_info(&im_size, &im_width, &im_height, &im_planes);
    build_gui();
    network_start();

    static char path[1024];
    time_t t;
    time(&t);
    char const *capture = get_setting("capture", "/var/tmp/pilot/capture");
    sprintf(path, "%s-%ld", capture, (long)t);
#if VERY_SLOW_GL
    CaptureParams cap = {
        640, 480, path, &buffer_callback, NULL
    };
    setup_capture(&cap);
#endif
    fprintf(stderr, "capture directed to %s\n", capture);

    char const *tty = get_setting("serial", "/dev/ttyACM0");
    int speed = get_setting_int("serialbaud", 1000000);
    if (!start_serial(tty, speed)) {
        fprintf(stderr, "Could not open serial port %s: %s\n", tty, strerror(errno));
        exit(1);
    }

    commsRunning = true;
    pthread_create(&commsThread, NULL, comms_thread, NULL);
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


bool nomarg(int *pac, char const ***pav, char const *arg) {
    for (int i = 1; i != *pac; ++i) {
        if (!strcmp(arg, (*pav)[i])) {
            memcpy(&(*pav)[i], &(*pav)[i+1], (*pac - i) * sizeof(char const *));
            *pac -= 1;
            return true;
        }
    }
    return false;
}

int main(int argc, char const *argv[]) {
    signal(SIGINT, do_int);
    google::InitGoogleLogging("pilot");
    mkdir("/var/tmp/pilot", 0775);
    if (nomarg(&argc, &argv, "--logfile")) {
        char logpath[128];
        time_t t;
        time(&t);
        sprintf(logpath, "/var/tmp/pilot/pilot-%ld-err.log", (long)t);
        FILE *q = freopen(logpath, "wb", stderr);
        if (!q) {
            perror(logpath);
        }
        sprintf(logpath, "/var/tmp/pilot/pilot-%ld-out.log", (long)t);
        q = freopen(logpath, "wb", stderr);
        if (!q) {
            perror(logpath);
        }
        set_setting_long("running_status", 0);
    }
    if (load_settings("pilot")) {
        fprintf(stderr, "Loaded settings from '%s'\n", "pilot");
    }
    if (gg_setup(get_setting_int("width", 1024), get_setting_int("height", 600), 0, 0, "pilot") < 0) {
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
    for (int i = 1; argv[i]; ++i) {
        char const *s = argv[i];
        if (s[0] == '-' && s[1] == '-') {
            s += 2;
            char const *p = strchr(s, '=');
            if (p) {
                std::string key(s, p);
                std::string value(p+1);
                set_setting(key.c_str(), value.c_str());
            }
        }
    }
    if (argc != 2 || strcmp(argv[1], "--test")) {
        gg_onmousebutton(do_click);
        gg_onmousemove(do_move);
        gg_ondraw(do_draw);
        if (!configure_metrics()) {
            fprintf(stderr, "Can't run without metrics\n");
            return -1;
        }
        time_t curtime;
        time(&curtime);
        cleanup_temp_folder("/var/tmp/pilot", curtime-86400*3);
        setup_run();
        gg_run(do_idle);
        stop_capture();
    }
    commsRunning = false;
    stop_serial();
    network_stop();
    stop_metrics();
    int num = 0;
    gg_get_gl_errors(enum_errors, &num);
    exit((num > 0) ? 1 : 0);
    return 0; /*NOTREACHED*/
}

