
LFLAGS:=-L/usr/local/lib
CFLAGS:=-I/usr/local/include
PYTHON:=python2

obj/flatshow:	obj/flatshow.o obj/table.o
	g++ $(LFLAGS) -g -o $@ $^ -lopencv_core -lopencv_imgproc -lopencv_highgui # -lopencv_imgcodecs

clean:
	rm -rf obj

obj/%.o:	%.cpp
	-mkdir -p obj
	g++ $(CFLAGS) -c -g -o $@ $< -MMD -std=gnu++11

table.cpp:	maketable_rect.py calibrate.pkl localcrop.py
	$(PYTHON) maketable_rect.py
#table.cpp:	maketable_flat.py calibrate.pkl localcrop.py
#	python2 maketable_flat.py

-include $(wildcard obj/*.d)
