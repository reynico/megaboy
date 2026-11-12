GBDK_HOME ?= $(HOME)/opt/gbdk
LCC := $(GBDK_HOME)/bin/lcc
ROM := build/megaboy.gbc
SRC := $(wildcard src/*.c) ../common/link.c

.PHONY: all test clean

all: $(ROM)

src/font.c: ../tools/mkfont.py
	python3 -I ../tools/mkfont.py $@

$(ROM): $(SRC) src/font.c $(wildcard src/*.h) ../common/link.h
	@mkdir -p build
	$(LCC) -I../common -Wm-yc -Wm-yt0x1B -Wm-ya1 -Wm-yn"MEGABOY" -o $@ $(SRC)

test: src/font.c
	@mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -Isrc src/fmt.c src/sim.c test/test_logic.c -o build/test_logic
	./build/test_logic

clean:
	rm -rf build
