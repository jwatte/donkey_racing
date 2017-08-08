#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../stb/stb_image_write.h"



//  320 pixels wide, 60 degree H-FOV at 1 unit
#define VP_WIDTH 0.57735f
//  240 pixels tall, 60 degree H-FOV at 1 unit
#define VP_HEIGHT 0.43301f

#define VP_NEAR 0.1f
#define VP_FAR 10.0f
#define CAM_HEIGHT 0.2f



void error_cb(int error, char const *msg) {
    fprintf(stderr, "ERROR: %d: %s\n", error, msg);
    exit(4);
}

GLuint noise_tx;

void make_noise_texture() {
    unsigned char *buf = (unsigned char *)malloc(128*128*4);
    for (int i = 0; i != 128*128*4; i += 4) {
        float u = drand48() + 0.7f;
        float v = drand48() + 0.7f;
        float y = drand48() * 0.1f + 0.1f;
        buf[i + 0] = (unsigned char)(255 * y * u) + 150;
        buf[i + 1] = (unsigned char)(255 * y) + 150;
        buf[i + 2] = (unsigned char)(255 * y * v) + 150;
        buf[i + 3] = ((lrand48() >> 23) & 0x3f) | 0xC0;
    }
    stbi_write_png("noise.png", 128, 128, 4, buf, 0);
    glGenTextures(1, &noise_tx);
    glBindTexture(GL_TEXTURE_2D, noise_tx);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    assert(!glGetError());
    free(buf);
}

GLuint ofs_fb;
GLuint ofs_db;
GLuint ofs_tx;

void make_offscreen() {
    glGenFramebuffers(1, &ofs_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ofs_fb);
    glGenTextures(1, &ofs_tx);
    assert(!glGetError());
    glBindTexture(GL_TEXTURE_2D, ofs_tx);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ofs_tx, 0);
    assert(!glGetError());
    glGenRenderbuffers(1, &ofs_db);
    glBindRenderbuffer(GL_RENDERBUFFER, ofs_db);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ofs_db);
    assert(!glGetError());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    assert(!glGetError());
}

void draw_random_spots() {
    float mat[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mat);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, noise_tx);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.6f);
    for (int i = 0; i != 2000; ++i) {
        float y = drand48() * -8.0f;
        float x = drand48() * 10.0f - 5.0f;
        float w = drand48() * (0.02f + fabsf(y/50)) + 0.005f;
        float h = drand48() * (0.02f + fabsf(y/50)) + 0.005f;
        float o = drand48() * 90 - 45;
        glLoadMatrixf(mat);
        glRotatef(o, 0, 1, 0);
        float cr = drand48() * 0.6f;
        float cg = drand48() * 0.6f;
        float cb = drand48() * 0.6f;
        float ca = drand48();
        glColor4f(cr, cg, cb, ca);
        float u = (i % 40) / 40.0f;
        float v = ((i / 40) % 40) / 40.0f;
        glBegin(GL_QUADS);
        glTexCoord2f(u, v);
        glVertex3f(x-w, 0.001f, y-h);
        glTexCoord2f(u + 0.02f, v);
        glVertex3f(x+w, 0.001f, y-h);
        glTexCoord2f(u + 0.02f, v + 0.02f);
        glVertex3f(x+w, 0.001f, y+h);
        glTexCoord2f(u, v + 0.02f);
        glVertex3f(x-w, 0.001f, y+h);
        glEnd();
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    assert(!glGetError());
}

float curve_near = 0.0f;
float curve_slant = 0.0f;
float curve_far = 0.0f;
float curve_phase = 0.0f;

float i_near;
float i_slant;
float i_far;
float i_phase;

void rotate(float slant, float cx, float cy, float dx, float dy, float u, float v) {
    float ss = sinf(slant);
    float cs = cosf(slant);
    glTexCoord2f(u, v);
    glVertex3f(-cx - dx * cs - dy * ss, 0.001f, -cy - dy * cs + dx * ss);
}

void draw_a_curve() {
    i_near = curve_near;
    i_slant = curve_slant;
    i_far = curve_far;
    i_phase = curve_phase;
    float cx = curve_near;
    float cy = curve_phase;
    float slant = curve_slant;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, noise_tx);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_ALPHA_TEST);
    glColor4f(0.8f, 0.7f, 0.3f, 1.0f);
    for (int i = 0; i != 20; ++i) {
        glAlphaFunc(GL_GREATER, 0.77f + drand48() * 0.10f);
        glBegin(GL_QUADS);
        float w = drand48() * 0.01f + 0.02f;
        float h = drand48() * 0.03f + 0.02f;
        rotate(slant, cx, cy, -w, -h, i/20.0f, i/20.0f);
        rotate(slant, cx, cy, w, -h, (i+1)/20.0f, i/20.0f);
        rotate(slant, cx, cy, w, h, (i+1)/20.0f, (i+1)/20.0f);
        rotate(slant, cx, cy, -w, h, i/20.0f, (i+1)/20.0f);
        glEnd();
        cx += sinf(slant) * 0.24f;
        cy += cosf(slant) * 0.24f;
        slant += (curve_far - cx) / (20 - i);
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
}

void draw_the_scene() {
    glClearColor(0, 0, 0.5f, 0);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);

    //  set up camera

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-VP_WIDTH * VP_NEAR * 512 / 320, VP_WIDTH * VP_NEAR * 512 / 320,
            -VP_HEIGHT * VP_NEAR * 512 / 240, VP_HEIGHT * VP_NEAR * 512 / 240,
            VP_NEAR, VP_FAR);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -CAM_HEIGHT, 0.0f);

    //  draw stuff

    //  draw the floor
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, noise_tx);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBegin(GL_QUADS);
    glColor4f(0.15f, 0.15f, 0.15f, 1.0f);
    float du = drand48();
    float dv = drand48();
    glTexCoord2f(du-10.0f, dv-10.0f);
    glVertex3f(-10.0f, 0.0f, 10.0f);
    glTexCoord2f(du+10.0f, dv-10.0f);
    glVertex3f(10.0f, 0.0f, 10.0f);
    glTexCoord2f(du+10.0f, dv+10.0f);
    glVertex3f(10.0f, 0.0f, -10.0f);
    glTexCoord2f(du-10.0f, dv+10.0f);
    glVertex3f(-10.0f, 0.0f, -10.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    //  confetti on the floor
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    draw_random_spots();
    glDisable(GL_BLEND);

    //  draw a curve
    curve_phase = drand48() * 0.25f;
    curve_near = drand48() - 0.5f;
    curve_far = 10.0f * (drand48() - 0.5f);
    curve_slant = 0.5f * (drand48() - 0.5f);
    draw_a_curve();

}

unsigned char picture_data[240][320][3];

void render_offscreen() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ofs_fb);

    draw_the_scene();

    glFinish();
    assert(!glGetError());

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    assert(!glGetError());
}

void draw_offscreen_to_window() {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    assert(!glGetError());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBindTexture(GL_TEXTURE_2D, ofs_tx);
    assert(!glGetError());

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, -.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    assert(!glGetError());
}



void flip_pixels() {
    unsigned char row[320][3];
    for (int i = 0; i != 120; ++i) {
        memcpy(row, picture_data[i], 320*3);
        memcpy(picture_data[i], picture_data[239-i], 320*3);
        memcpy(picture_data[239-i], row, 320*3);
    }
}

bool is_line_pix(unsigned char *p) {
    return (p[0] >= 150 && p[1] >= 120 && p[2] < 100);
}

bool is_there_a_line() {
    int npix = 0;
    for (int r = 0; r != 240; ++r) {
        for (int c = 0; c != 320; ++c) {
            if (is_line_pix(picture_data[r][c])) {
                ++npix;
            }
        }
    }
    return npix > 20;
}

void mark_pix() {
    for (int r = 0; r != 240; ++r) {
        for (int c = 0; c != 320; ++c) {
            if (is_line_pix(picture_data[r][c])) {
                picture_data[r][c][0] = 255;
                picture_data[r][c][1] = 128;
                picture_data[r][c][2] = 255;
            }
        }
    }
}

int nwritten = 0;
FILE *labels = stdout;

void write_image(bool is_line) {
    char path[200];
    ++nwritten;
    sprintf(path, "img_%05d_%d_%.2f_%.2f_%.2f_%.2f.png",
        nwritten, is_line ? 1 : 0, i_near - 0.005f, i_far - 0.005f, i_slant - 0.005f, i_phase - 0.005f);
    fprintf(labels, "%s,%d,%d,%.3f,%.3f,%.3f,%.3f\n", path, nwritten, is_line ? 1 : 0, i_near, i_far, i_slant, i_phase);
    flip_pixels();
    stbi_write_png(path, 320, 240, 3, picture_data, 0);
    mark_pix();
    sprintf(path, "img_%05d_%d_line.png", nwritten, is_line ? 1 : 0);
    stbi_write_png(path, 320, 240, 3, picture_data, 0);
}


int nToWrite = 100;

int main(int argc, char const *argv[])
{
    if (argc == 2) {
        nToWrite = atoi(argv[1]);
        if (nToWrite < 1) {
            fprintf(stderr, "bad number to write: %s\n", argv[1]);
            exit(1);
        }
    }
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit() failed: no graphcis card available?\n");
        return 1;
    }

    glfwSetErrorCallback(error_cb);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(512, 512, "Hello World", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "glfwCreateWindow() failed: no display available?\n");
        glfwTerminate();
        return 2;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "GLEW failed to initialize: OpenGL not supported?\n");
        return 3;
    }

    make_noise_texture();

    char const *v = (char const *)glGetString(GL_VERSION);
    fprintf(stderr, "GL version: %s\n", v);

    make_offscreen();

    srand48(1);
    labels = fopen("labels.txt", "wb");
    if (!labels) {
        fprintf(stderr, "Could not create labels.txt\n");
        exit(1);
    }

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        assert(!glGetError());
        render_offscreen();
        assert(!glGetError());

        glfwMakeContextCurrent(window);
        assert(!glGetError());

        draw_offscreen_to_window();

        glReadPixels((512-320)/2, (512-240)/2, 320, 240, GL_RGB, GL_UNSIGNED_BYTE, picture_data);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        assert(!glGetError());

        write_image(is_there_a_line());
        if (nwritten == nToWrite) {
            break;
        }

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

