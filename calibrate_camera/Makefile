
LFLAGS:=-L/usr/local/lib
CFLAGS:=-I/usr/local/include
PYTHON:=python2

all:	obj/flatshow
	obj/flatshow img/race2.png # img/squares2.png

obj/flatshow:	obj/flatshow.o obj/flat_table.o
	g++ $(LFLAGS) -g -o $@ $^ -lopencv_core -lopencv_imgproc -lopencv_highgui -MMD -lopencv_imgcodecs

clean:
	rm -rf obj

obj/%.o:	%.cpp
	-mkdir -p obj
	g++ $(CFLAGS) -c -g -o $@ $< -MMD -std=gnu++11

flat_table.cpp:	maketable_flat.py calibrate.pkl localcrop.py
	$(PYTHON) maketable_flat.py
table.cpp:	maketable_rect.py calibrate.pkl localcrop.py
	$(PYTHON) maketable_rect.py
#table.cpp:	maketable_flat.py calibrate.pkl localcrop.py
#	python2 maketable_flat.py

-include $(wildcard obj/*.d)

obj/flatshow.o:	flat_table.cpp
