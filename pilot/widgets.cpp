#include "widgets.h"
#include "artist.h"
#include "glesgui.h"



constexpr float BTOF_FACTOR = 1.0f / 255.0f;
constexpr uint32_t FTOB_FACTOR = 255.0f;

static float btof(uint32_t b) {
    return (b & 0xff) * BTOF_FACTOR;
}

static uint32_t ftob(float f) {
    if (f < 0) return 0;
    if (f > 1) return 255;
    return uint32_t(f * FTOB_FACTOR);
}

Dimension::Dimension() {
    val_ = prop_ = 0;
}

Dimension::Dimension(float val) {
    val_ = val;
    prop_ = 0;
}

Dimension::Dimension(float val, float prop) {
    val_ = val;
    prop_ = prop;
}

float Dimension::calc(float spread, float totalProp) {
    if (!totalProp || !prop_) {
        return val_;
    }
    return val_ + prop_ * spread / totalProp;
}

Color::Color() {
    r = 1;
    g = 1;
    b = 1;
    a = 1;
}

Color::Color(uint32_t argb) {
    setArgb(argb);
}

Color::Color(float rr, float gg, float bb, float aa) {
    r = rr;
    g = gg;
    b = bb;
    a = aa;
}

Color::Color(Color const &col, float aa) {
    *this = col;
    a = aa;
}


uint32_t Color::asArgb() {
    return ftob(b) | (ftob(g) << 8) | (ftob(r) << 16) | (ftob(a) << 24);
}

Color &Color::setArgb(uint32_t argb) {
    r = btof(argb >> 16);
    g = btof(argb >> 8);
    b = btof(argb);
    a = btof(argb >> 24);
    return *this;
}

Color &Color::set(float rr, float gg, float bb, float aa) {
    r = rr;
    g = gg;
    b = bb;
    a = aa;
    return *this;
}

static Color defbg(0.8f, 0.8f, 0.8f);
static Color deffg(0.0f, 0.0f, 0.0f);
static Color defa1(0.7f, 0.4f, 0.1f);
static Color defa2(0.4f, 0.1f, 0.7f);

DrawStyle DrawStyle::common = { defbg, deffg, defa1, defa2, nullptr };
DrawStyle DrawStyle::disabled = { defbg, Color(deffg, 0.2f), Color(defa1, 0.2f), Color(defa2, 0.2f), nullptr };
DrawStyle DrawStyle::highlighted = { Color(0.95f, 0.95f, 0.95f), Color(0.5f, 0.0f, 0.0f), defa1, defa2, nullptr };




