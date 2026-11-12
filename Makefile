CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude
LDLIBS += -lm

.PHONY: all test clean
all: build/test_dsp_behavior build/trace_demo

build:
	mkdir -p build

build/test_dsp_behavior: src/dsp_behavior.c src/pfc_control.c tests/test_dsp_behavior.c include/dsp_behavior.h include/pfc_control.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/dsp_behavior.c src/pfc_control.c tests/test_dsp_behavior.c $(LDLIBS) -o $@

build/trace_demo: src/dsp_behavior.c src/pfc_control.c examples/trace_demo.c include/dsp_behavior.h include/pfc_control.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/dsp_behavior.c src/pfc_control.c examples/trace_demo.c $(LDLIBS) -o $@

test: build/test_dsp_behavior
	./build/test_dsp_behavior

clean:
	rm -rf build
