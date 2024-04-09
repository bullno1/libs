.PHONY: all

all: bin/autolist bin/xincbin

bin/autolist: tests/autolist/a.c tests/autolist/b.c tests/autolist/main.c
	mkdir -p bin
	$(CC) $^ -o $@

bin/xincbin: tests/xincbin/main.c tests/xincbin/resources.c
	mkdir -p bin
	$(CC) -Itests/xincbin $^ -o $@
