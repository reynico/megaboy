GBDK_HOME ?= $(HOME)/opt/gbdk
LCC := $(GBDK_HOME)/bin/lcc
SAN ?= undefined
ROM := build/megaboy.gbc
SRC := $(wildcard src/*.c) ../common/link.c ../common/rtc.c ../common/tune.c

.PHONY: all test clean

all: $(ROM)

src/font.c: ../tools/mkfont.py
	python3 -I ../tools/mkfont.py $@

src/gauge_gfx.c: ../tools/mkgauge.py ../tools/mkfont.py
	python3 -I ../tools/mkgauge.py $@

$(ROM): $(SRC) src/font.c src/gauge_gfx.c $(wildcard src/*.h) ../common/link.h ../common/rtc.h ../common/tune.h
	@mkdir -p build
	$(LCC) -I../common -Wm-yc -Wm-yt0x1B -Wm-ya1 -Wm-yn"MEGABOY" -o $@ $(SRC)

test: src/font.c
	@mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=$(SAN) -Isrc src/fmt.c test/test_logic.c -o build/test_logic
	./build/test_logic
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=$(SAN) -Isrc -I../common src/tdata.c ../common/link.c ../common/rtc.c ../common/tune.c test/test_tdata.c -o build/test_tdata
	./build/test_tdata

clean:
	rm -rf build
