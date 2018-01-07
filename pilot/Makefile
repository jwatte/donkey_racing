
TOOLS:=rifftoh264 reset
CFILES:=$(wildcard *.c)
CPPFILES:=$(wildcard *.cpp)
C_O:=$(patsubst %.c,obj/%.o,$(CFILES))
CPP_O:=$(patsubst %.cpp,obj/%.o,$(CPPFILES))
LIBS:=-lpthread -Wl,--no-whole-archive -lCaffe2_CPU -lglog -lprotobuf
OPT?=-O2
CFLAGS:=-Wall -Werror $(OPT) -I/usr/local/include -I/media/pi/localsrc/caffe2/third_party/eigen
CPPFLAGS:=$(CFLAGS) -std=gnu++11 -Wno-deprecated
TOOL_O:=$(patsubst %,obj/%.o,$(TOOLS))
PILOT_O:=$(filter-out $(TOOL_O),$(C_O) $(CPP_O))
ifeq ($(wildcard /opt/vc),)
else
CFLAGS+=-I/opt/vc/include
CPPFLAGS+=-I/opt/vc/include
LIBS+=-L/opt/vc/lib -lvcos -lmmal -lmmal_core -lmmal_util -lbcm_host -L/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL -lX11
endif



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

OBJ_riff2h264:=
OBJ_reset:=obj/metric.o obj/crckrmit.o obj/serial.o obj/settings.o obj/metrics.o

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
	g++ -g -o $@ $(PILOT_O) $(LIBS) $(OPT) -std=gnu++11

define build_tool =
$(1):	obj/$(1).o
	g++ -g -o $(1) obj/$(1).o $(LIBS) $(OPT) $(OBJ_$(1)) -std=gnu++11
endef

$(foreach TOOL,$(TOOLS),$(eval $(call build_tool,$(TOOL))))

clean:
	rm -rf obj $(TOOLS) pilot gmon.out prof.txt

obj/%.o:	%.c
	@mkdir -p obj
	gcc -g -c -o $@ $< -MMD $(CFLAGS) -std=gnu11

obj/%.o:	%.cpp
	@mkdir -p obj
	g++ -g -c -o $@ $< -MMD $(CPPFLAGS)

-include $(patsubst %.o,%.d,$(C_O) $(CPP_O))
