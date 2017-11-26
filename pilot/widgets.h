#if !defined(widgets_h)
#define widgets_h

#include <vector>
#include <stdint.h>
#include <stddef.h>

class Font;
class Artist;

class Dimension {
    public:
        Dimension();
        Dimension(float val);
        Dimension(float val, float prop);

        float val_;
        float prop_;

        float calc(float spread, float totalProp);
};

enum class LayoutType {
    none,
    position,
    horizontal,
    vertical
};

struct Color {
    Color();
    Color(uint32_t argb);
    Color(float rr, float gg, float bb, float aa = 1.0f);
    Color(Color const &c, float aa);

    uint32_t asArgb();
    Color &setArgb(uint32_t argb);
    Color &set(float rr, float gg, float bb, float aa = -1.0f);

    float r;
    float g;
    float b;
    float a;
};

struct DrawStyle {

    Color background;
    Color foreground;
    Color accent1;
    Color accent2;
    Font *font;

    static DrawStyle common;
    static DrawStyle disabled;
    static DrawStyle highlighted;
};

class Widget {
    public:
        Widget(Dimension width, Dimension height, LayoutType layout = LayoutType::position, Dimension left = Dimension(), Dimension bottom = Dimension());
        virtual ~Widget();

        virtual void draw();
        virtual void setDrawStyleRef(DrawStyle *ds);
        virtual void setDrawStyle(DrawStyle ds);
        virtual bool hasForeground();
        bool setHasForeground(bool v);

        virtual void mouseEnter();
        virtual void mouseLeave();
        virtual void mouseMove(float yourx, float youry);
        virtual void mouseClick(unsigned char buttonMask);
        virtual void mouseRelease(unsigned char buttonMask);

        virtual void postLayout(float xpos, float ypos, float xsize, float ysize);

        virtual size_t addChild(Widget *);
        virtual bool removeChild(Widget *, bool dispose=true);
        virtual Widget *childAt(size_t ix);
        virtual bool removeChildAt(size_t ix, bool dispose=true);
        virtual size_t countChildren();
        virtual void removeAllChildren(bool dispose=true);

        virtual Widget *parent();

    protected:

        friend class Artist;

        LayoutType layout;
        Widget *parent_;
        std::vector<Widget *> children_;
        DrawStyle *drawStyle_;
        bool drawStyleOwned_;
        bool hasForeground_;

        /* this area is defined after layout has completed -- it's in actual screen units */
        float xpos_;
        float ypos_;
        float xsize_;
        float ysize_;

        /* these are the parameters that go into layout -- define minimum size and stretch */
        Dimension width_;
        Dimension height_;
        Dimension left_;
        Dimension bottom_;
};

#endif  //  widgets_h

