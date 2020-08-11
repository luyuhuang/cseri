all : cseri.so

cseri.so: binary.c buffer.c cseri.c text.c
	gcc -O2 -std=gnu99 -Wall -Wextra -fPIC --shared $^ -o $@

clean:
	rm -f *.o *.so

test:
	lua test.lua

.PHONY: all test clean
