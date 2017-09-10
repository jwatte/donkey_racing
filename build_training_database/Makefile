
CPPSRC:=$(wildcard *.cpp) ../pilot/image.cpp
OBJ:=$(patsubst %.cpp,obj/%.o,$(notdir $(CPPSRC)))
OPT?=-O0 -mtune=corei7-avx
CFLAGS:=-I/usr/local/include -I/usr/local/src/caffe2 -I/usr/local/src/caffe2/third_party/eigen -g -std=gnu++11 -Wall -Werror -Wno-sign-compare -MMD $(OPT)
LFLAGS:=-g -std=gnu++11 $(OPT) -Wall -Werror -L/usr/local/src/caffe2/build/caffe2  -L/usr/local/lib $(sort -lavcodec -lX11 -lasound -lx265 -lx264 -lm -lz -pthread -lavresample -lm -lavutil -lm -lCaffe2_GPU)

build_training_database:	$(OBJ)
	g++ -o build_training_database $(OBJ) $(OPT) $(LFLAGS)

obj/%.o:	%.cpp
	-mkdir -p obj
	g++ -c -o $@ $< $(CFLAGS)

obj/%.o:	../calibrate_camera/%.cpp
	-mkdir -p obj
	g++ -c -o $@ $< $(CFLAGS)

obj/%.o:	../pilot/%.cpp
	-mkdir -p obj
	g++ -c -o $@ $< $(CFLAGS)

clean:
	rm -f build_training_database obj

-include $(patsubst %.o,%.d,$(OBJ))