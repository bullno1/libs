.PHONY: all clean

all: doc/index.html bin/autolist bin/xincbin bin/mem_layout bin/barena bin/tlsf bin/bresmon bin/bhash bin/bcoro

clean:
	rm -rf bin
	rm -rf doc

doc/index.html: Doxyfile
	doxygen

bin/autolist: tests/autolist/a.c tests/autolist/b.c tests/autolist/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@

bin/xincbin: tests/xincbin/main.c tests/xincbin/resources.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/xincbin $^ -o $@

bin/mem_layout: tests/mem_layout/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/mem_layout $^ -o $@

bin/barena: tests/barena/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/barena $^ -o $@

bin/tlsf: tests/tlsf/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/tlsf $^ -o $@

bin/bresmon: tests/bresmon/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bresmon $^ -o $@

bin/bhash: tests/bhash/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bhash $^ -o $@

bin/bcoro: tests/bcoro/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bcoro $^ -o $@
