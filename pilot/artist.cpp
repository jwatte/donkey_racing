#include "artist.h"
#include "widgets.h"
#include "glesgui.h"
#include <algorithm>
#include <GLES2/gl2.h>
#include <math.h>


static bool clip(float left, float bottom, float width, float height, Aperture &ap) {
    float l = std::max(left, ap.left);
    float b = std::max(bottom, ap.bottom);
    float w = std::min(left+width, ap.left+ap.width)-l;
    float h = std::min(bottom+height, ap.bottom+ap.height)-b;
    ap.left = l;
    ap.bottom = b;
    ap.width = w;
    ap.height = h;
    return w > 0 && h > 0;
}

Artist::Artist() {
}

Artist::~Artist() {
}

void Artist::drawTopWidget(Widget *w) {
    gg_get_viewport(&swidth_, &sheight_);
    Aperture a = { 0, 0, float(swidth_), float(sheight_) };
    descendWidget(w, a);
}

void Artist::descendWidget(Widget *w, Aperture ap) {
    if (clip(w->xpos_, w->ypos_, w->xsize_, w->ysize_, ap)) {
        clearBackground(w, ap);
        for (auto const &cp : w->children_) {
            descendWidget(cp, ap);
        }
        if (w->hasForeground()) {
            glScissor(floorf(ap.left), floorf(ap.bottom), ceilf(ap.width), ceilf(ap.height));
            glEnable(GL_SCISSOR_TEST);
            w->draw();
            glDisable(GL_SCISSOR_TEST);
        }
    }
}

void Artist::clearBackground(Widget *w, Aperture const &ap) {
    Color c = DrawStyle::common.background;
    if (w->drawStyle_) {
        c = w->drawStyle_->background;
    }
    if (c.a > 0) {
        gg_draw_box(floorf(ap.left), floorf(ap.bottom), ceilf(ap.left + ap.width), ceilf(ap.bottom + ap.height), c.asArgb());
    }
}


