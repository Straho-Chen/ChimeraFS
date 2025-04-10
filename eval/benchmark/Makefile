all: default

default:
	if [ -d "build" ] && [ -f "build/Makefile" ]; then cd build && make -i app-clean && cd -; fi
	if [ -d "build" ]; then rm -rf build; fi
	mkdir -p build && cd build && cmake .. && make