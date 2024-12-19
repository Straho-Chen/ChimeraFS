CFLAGS := -O3

TARGETS := to_MiB_s

all: ${TARGETS}
	
%: %.c
	gcc $^ -O3 -o $@

%: %.cpp
	g++ $^ -O3 -o $@

clean:
	rm ${TARGETS}

.PHONY: all clean
