CFLAGS := -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -Istub -I../../common

.PHONY: test

test: build/test_datalog
	./build/test_datalog

build/test_datalog: test_datalog.c ../src/datalog.c ../src/datalog.h $(wildcard stub/*.h stub/pico/*.h stub/pico/util/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) test_datalog.c ../../common/link.c ../../common/rtc.c -o $@
