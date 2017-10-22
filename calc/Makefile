
CPPSRC:=$(wildcard *.cpp)
TOOLS:=$(patsubst %.cpp,obj/%,$(CPPSRC))

all:	$(TOOLS)

clean:	rm -rf obj

obj/%:	%.cpp
	-mkdir -p obj
	g++ -o $@ $< -std=c++11 -Wall -Werror -MMD

-include $(patsubst %,%.d,$(TOOLS))
