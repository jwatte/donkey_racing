
CPPSRC:=$(wildcard *.cpp) ../pilot/image.cpp
CSRC:=../pilot/yuv.c
CPPOBJ:=$(patsubst %.cpp,obj/%.o,$(notdir $(CPPSRC)))
COBJ:=$(patsubst %.c,obj/%.o,$(notdir $(CSRC)))
OBJ:=$(CPPOBJ) $(COBJ)
BTOBJ:=$(filter-out obj/mkdbfromstream.o obj/conefind.o,$(OBJ))
OPT?=-O3
OPT+=-mtune=corei7-avx
CFLAGS:=-I/usr/local/include -I/usr/local/src/caffe2 -I/usr/local/src/caffe2/third_party/eigen -g -Wall -Werror -Wno-sign-compare -MMD $(OPT)
CPPFLAGS:=$(CFLAGS)  -std=gnu++11
LFLAGS:=-g -std=gnu++11 $(OPT) -Wall -Werror -L/usr/local/src/caffe2/build/caffe2  -L/usr/local/lib $(sort -lavcodec -lX11 -lasound -lx265 -lx264 -lm -lz -pthread -lavresample -lm -lavutil -lm -lprotobuf) -lCaffe2_GPU -lCaffe2_CPU -lglog -lgflags -lomp

all:	build_training_database mkdbfromstream conefind

build_training_database:	$(BTOBJ)
	g++ -o build_training_database $(BTOBJ) $(OPT) $(LFLAGS)

mkdbfromstream:	obj/mkdbfromstream.o
	g++ -o mkdbfromstream obj/mkdbfromstream.o $(OPT) $(LFLAGS)

conefind:	obj/conefind.o obj/cone.o obj/stb.o
	g++ -o conefind $^ $(OPT) $(LFLAGS)

obj/%.o:	%.cpp Makefile
	@mkdir -p obj
	g++ -c -o $@ $< $(CPPFLAGS)

obj/%.o:	../calibrate_camera/%.cpp Makefile
	@mkdir -p obj
	g++ -c -o $@ $< $(CPPFLAGS)

obj/%.o:	../pilot/%.cpp Makefile
	@mkdir -p obj
	g++ -c -o $@ $< $(CPPFLAGS)

obj/%.o:	../pilot/%.c Makefile
	@mkdir -p obj
	gcc -c -o $@ $< $(CFLAGS)

clean:
	rm -rf build_training_database obj

-include $(patsubst %.o,%.d,$(OBJ))
