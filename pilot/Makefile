
TOOLS:=
CFILES:=$(wildcard *.c)
CPPFILES:=$(wildcard *.cpp)
C_O:=$(patsubst %.c,obj/%.o,$(CFILES))
CPP_O:=$(patsubst %.cpp,obj/%.o,$(CPPFILES))
LIBS:=-L/opt/vc/lib -lvcos -lmmal -lmmal_core -lmmal_util -lbcm_host -lpthread -Wl,--no-whole-archive -L/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL
OPT?=-O0
CFLAGS:=-I ../moneypit_v/mpv_teensy -I/opt/vc/include -Wall -Werror $(OPT)
CPPFLAGS:=$(CFLAGS) -std=gnu++11
TOOL_O:=$(patsubst %,obj/%.o,$(TOOLS))
PILOT_O:=$(filter-out $(TOOL_O),$(C_O) $(CPP_O))


# Some rules for automatically syntax checking vertex/fragment shader pairs
VSHADERS:=$(wildcard data/*.vertex.glsl)
FSHADERS:=$(wildcard data/*.fragment.glsl)
ifeq ($(VSHADERS),)
$(error "no vertex shaders found")
endif
ifeq ($(FSHADERS),)
$(error "no fragment shaders found")
endif
VSHADERNAMES:=$(patsubst data/%.vertex.glsl,%,$(VSHADERS))
FSHADERNAMES:=$(patsubst data/%.fragment.glsl,%,$(FSHADERS))
ifeq ($(VSHADERNAMES),)
$(error "Could not calculate vertex shader names")
endif
ifeq ($(FSHADERNAMES),)
$(error "Could not calculate fragment shader names")
endif
TEXTURES:=$(wildcard data/*.jpg) $(wildcard data/*.png) $(wildcard data/*.tga)
TEXTURENAMES:=$(patsubst data/%.jpg,%,$(patsubst data/%.png,%,$(patsubst data/%.tga,%,$(TEXTURES))))
MESHES:=$(wildcard data/*.mesh)
MESHNAMES:=$(patsubst data/%.mesh,%,$(MESHES))


#test:	mkdetect mkchecker
#	#./mkchecker > checker.yuv
#	#./mkpng checker.yuv yuv
#	-rm -f /tmp/foo.png /tmp/square.png raw.png
#	./mkdetect dump /tmp/foo.png sample.yuv
#	feh /tmp/foo.png /tmp/square.png

all:	pilot $(TOOLS) test_compile
	./pilot --test 2>&1 | sed -e 's/^pilot: //' 1>&2

test_compile:	pilot
	./pilot $(patsubst %,--compile %,$(sort $(VSHADERNAMES) $(FSHADERNAMES))) \
		$(patsubst %,--loadmesh %,$(sort $(MESHNAMES))) \
		$(patsubst %,--loadtexture %,$(sort $(TEXTURENAMES)))

pilot:	$(PILOT_O)
	g++ -g -o $@ $(PILOT_O) $(LIBS) -std=gnu++11

mkpng:	obj/mkpng.o obj/imagewrite.o obj/yuv.o
	gcc -g -o $@ $^ -std=gnu11 -lm

mkyuv:	obj/mkyuv.o obj/imagewrite.o obj/yuv.o
	g++ -g -o $@ $^ -std=gnu++11 -lm

mkdetect:	obj/mkdetect.o obj/imagewrite.o obj/yuv.o obj/detect_inner.o obj/settings.o obj/project.o obj/queue.o
	g++ -g -o $@ $^ -std=gnu++11 -lm -lefence

mkchecker:	obj/mkchecker.o
	g++ -g -o $@ $^ -std=gnu++11 -lm

clean:
	rm -rf obj $(TOOLS)

obj/%.o:	%.c
	@mkdir -p obj
	gcc -g -c -o $@ $< -MMD $(CFLAGS) -std=gnu11

obj/%.o:	%.cpp
	@mkdir -p obj
	g++ -g -c -o $@ $< -MMD $(CPPFLAGS)

-include $(patsubst %.o,%.d,$(C_O) $(CPP_O))
