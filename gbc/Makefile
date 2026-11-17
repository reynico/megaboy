GBDK_HOME ?= $(HOME)/opt/gbdk
LCC := $(GBDK_HOME)/bin/lcc
SAN ?= undefined
ROM := build/megaboy.gbc
SRC := $(wildcard src/*.c) ../common/link.c ../common/rtc.c ../common/tune.c
VERSION := $(shell v="$${MB_VERSION:-$$(git describe --tags --exact-match 2>/dev/null)}"; \
	printf '%s' "$${v:-dev}" | tr -cd 'A-Za-z0-9.-' | tr a-z A-Z | cut -c1-12)
COMMIT := $(shell git rev-parse --short=7 HEAD 2>/dev/null | tr -cd '0-9a-f' | tr a-f A-F)
COMMIT := $(if $(COMMIT),$(COMMIT),UNKNOWN)

.PHONY: all test clean FORCE

all: $(ROM)

src/font.c: ../tools/mkfont.py
	python3 -I ../tools/mkfont.py $@

src/gauge_gfx.c: ../tools/mkgauge.py ../tools/mkfont.py
	python3 -I ../tools/mkgauge.py $@

build/version.h: FORCE
	@mkdir -p build
	@printf '#define MB_VERSION "%s"\n#define MB_COMMIT "%s"\n' '$(VERSION)' '$(COMMIT)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

$(ROM): $(SRC) src/font.c src/gauge_gfx.c $(wildcard src/*.h) ../common/link.h ../common/rtc.h ../common/tune.h \
	build/version.h
	@mkdir -p build
	$(LCC) -I../common -Ibuild -Wm-yc -Wm-yt0x1B -Wm-ya1 -Wm-yn"MEGABOY" -o $@ $(SRC)

test: src/font.c
	@mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=$(SAN) -Isrc src/fmt.c test/test_logic.c -o build/test_logic
	./build/test_logic
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=$(SAN) -Isrc -I../common src/tdata.c ../common/link.c ../common/rtc.c ../common/tune.c test/test_tdata.c -o build/test_tdata
	./build/test_tdata

clean:
	rm -rf build
