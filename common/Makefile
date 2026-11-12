CFLAGS := -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -I.

.PHONY: test clean

test: build/test_ms1 build/test_ms2
	./build/test_ms1
	./build/test_ms2

build/test_ms1: ecu.c ms1.c ms1_tables.c test/test_ms1.c $(wildcard *.h)
	@mkdir -p build
	$(CC) $(CFLAGS) ecu.c ms1.c ms1_tables.c test/test_ms1.c -o $@

build/test_ms2: ecu.c ms2.c test/test_ms2.c $(wildcard *.h)
	@mkdir -p build
	$(CC) $(CFLAGS) ecu.c ms2.c test/test_ms2.c -o $@

clean:
	rm -rf build
