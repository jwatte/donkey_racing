
test:	blur
	cp source.png inout.png
	./blur inout.png mask.png
	feh *.png

blur:	blur.o stb.o
	g++ -std=gnu++11 -g -o blur blur.o stb.o

clean:
	rm -f blur *.o

%.o:	%.cpp
	g++ -std=gnu++11 -c -g -o $@ $< -O3 -Wall -Werror
