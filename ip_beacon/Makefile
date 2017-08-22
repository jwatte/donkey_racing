all:
	mkdir -p build
	cd build && cmake ..
	cd build && make

clean:
	rm -rf build

install:	all
	sudo bash ./install.sh

uninstall:
	sudo bash ./uninstall.sh
