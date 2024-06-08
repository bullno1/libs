.PHONY: all

all: bin/autolist bin/xincbin bin/mem_layout

bin/autolist: tests/autolist/a.c tests/autolist/b.c tests/autolist/main.c
	mkdir -p bin
	$(CC) $^ -o $@

bin/xincbin: tests/xincbin/main.c tests/xincbin/resources.c
	mkdir -p bin
	$(CC) -Itests/xincbin $^ -o $@

bin/mem_layout: tests/mem_layout/main.c
	mkdir -p bin
	$(CC) -Itests/mem_layout $^ -o $@
