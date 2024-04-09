.PHONY: all

all: bin/reglist bin/xincbin

bin/reglist: tests/reglist/a.c tests/reglist/b.c tests/reglist/main.c
	mkdir -p bin
	$(CC) $^ -o $@

bin/xincbin: tests/xincbin/main.c tests/xincbin/resources.c
	mkdir -p bin
	$(CC) -Itests/xincbin $^ -o $@
