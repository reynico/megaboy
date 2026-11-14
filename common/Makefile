CFLAGS := -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -I.

.PHONY: test clean

test: build/test_ms1 build/test_ms2 build/test_link build/test_tune build/test_rtc
	./build/test_ms1
	./build/test_ms2
	./build/test_link
	./build/test_tune
	./build/test_rtc

build/test_ms1: ecu.c ms1.c ms1_tables.c test/test_ms1.c $(wildcard *.h)
	@mkdir -p build
	$(CC) $(CFLAGS) ecu.c ms1.c ms1_tables.c test/test_ms1.c -o $@

build/test_ms2: ecu.c ms2.c test/test_ms2.c $(wildcard *.h)
	@mkdir -p build
	$(CC) $(CFLAGS) ecu.c ms2.c test/test_ms2.c -o $@

build/test_link: link.c test/test_link.c link.h rtc.h
	@mkdir -p build
	$(CC) $(CFLAGS) link.c test/test_link.c -o $@

build/test_tune: tune.c test/test_tune.c tune.h
	@mkdir -p build
	$(CC) $(CFLAGS) tune.c test/test_tune.c -o $@

build/test_rtc: rtc.c link.c test/test_rtc.c rtc.h link.h
	@mkdir -p build
	$(CC) $(CFLAGS) rtc.c link.c test/test_rtc.c -o $@

clean:
	rm -rf build
