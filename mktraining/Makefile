
CPP:=$(wildcard *.cpp)
O:=$(patsubst %.cpp,obj/%.o,$(CPP))
L:=-L/usr/local/lib -lglfw3 -ldl -lpthread -lX11 -lXrandr -lXinerama -lXcursor -lGL -lGLEW
CF:=-I/usr/local/include

mktraining:	$(O)
	g++ -std=gnu++11 -g -o $@ $(O) $(L)

clean:
	rm -rf obj img_* labels.txt

obj/%.o:	%.cpp
	-mkdir -p obj
	g++ $(CF) -MMD -std=gnu++11 -g -c -o $@ $<

-include $(patsubst %.o,%.d,$(O))
