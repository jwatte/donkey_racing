#if !defined(artist_h)
#define artist_h

#include <stdint.h>
#include <vector>

class Widget;
struct Color;
struct Texture;
struct Program;


struct Aperture {
    float left;
    float bottom;
    float width;
    float height;
};

class Artist {
    public:
        Artist();
        ~Artist();

        void drawTopWidget(Widget *w);

    protected:
        void descendWidget(Widget *w, Aperture ap);
        void clearBackground(Widget *w, Aperture const &ap);

        int swidth_;
        int sheight_;
};

#endif  //  artist_h

