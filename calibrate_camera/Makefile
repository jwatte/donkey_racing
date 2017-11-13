
LFLAGS:=-L/usr/local/lib
CFLAGS:=-I/usr/local/include

obj/flatshow:	obj/flatshow.o obj/table.o
	g++ $(LFLAGS) -g -o $@ $^ -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc

clean:
	rm -rf obj

obj/%.o:	%.cpp
	-mkdir -p obj
	g++ $(CFLAGS) -c -g -o $@ $< -MMD -std=gnu++11

table.cpp:	maketable_flat.py calibrate.pkl localcrop.py
	python maketable_flat.py

-include $(wildcard obj/*.d)
