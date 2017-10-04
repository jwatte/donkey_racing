
CPPSRC:=$(wildcard *.cpp) ../pilot/image.cpp
OBJ:=$(patsubst %.cpp,obj/%.o,$(notdir $(CPPSRC)))
BTOBJ:=$(filter-out obj/mkdbfromstream.o,$(OBJ))
OPT?=-O2
OPT+=-mtune=corei7-avx
CFLAGS:=-I/usr/local/include -I/usr/local/src/caffe2 -I/usr/local/src/caffe2/third_party/eigen -g -std=gnu++11 -Wall -Werror -Wno-sign-compare -MMD $(OPT)
LFLAGS:=-g -std=gnu++11 $(OPT) -Wall -Werror -L/usr/local/src/caffe2/build/caffe2  -L/usr/local/lib $(sort -lavcodec -lX11 -lasound -lx265 -lx264 -lm -lz -pthread -lavresample -lm -lavutil -lm -lprotobuf) -lCaffe2_GPU -lCaffe2_CPU -lglog -lgflags -lomp

all:	build_training_database mkdbfromstream

build_training_database:	$(BTOBJ)
	g++ -o build_training_database $(BTOBJ) $(OPT) $(LFLAGS)

mkdbfromstream:	obj/mkdbfromstream.o
	g++ -o mkdbfromstream obj/mkdbfromstream.o $(OPT) $(LFLAGS)

obj/%.o:	%.cpp Makefile
	@mkdir -p obj
	g++ -c -o $@ $< $(CFLAGS)

obj/%.o:	../calibrate_camera/%.cpp Makefile
	@mkdir -p obj
	g++ -c -o $@ $< $(CFLAGS)

obj/%.o:	../pilot/%.cpp Makefile
	@mkdir -p obj
	g++ -c -o $@ $< $(CFLAGS)

clean:
	rm -rf build_training_database obj

-include $(patsubst %.o,%.d,$(OBJ))
