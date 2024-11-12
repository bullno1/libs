.PHONY: all clean

all: \
	doc/index.html \
	bin/autolist \
	bin/xincbin \
	bin/mem_layout \
	bin/barena \
	bin/tlsf \
	bin/bresmon \
	bin/bhash \
	bin/bcoro \
	bin/bserial

clean:
	rm -rf bin
	rm -rf doc

doc/index.html: Doxyfile
	doxygen

bin/autolist: tests/autolist/a.c tests/autolist/b.c tests/autolist/main.c autolist.h
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@

bin/xincbin: tests/xincbin/main.c tests/xincbin/resources.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/xincbin $^ -o $@

bin/mem_layout: tests/mem_layout/main.c mem_layout.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/mem_layout $^ -o $@

bin/barena: tests/barena/main.c barena.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/barena $^ -o $@

bin/tlsf: tests/tlsf/main.c tlsf.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/tlsf $^ -o $@

bin/bresmon: tests/bresmon/main.c bresmon.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bresmon $^ -o $@

bin/bhash: tests/bhash/main.c bhash.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bhash $^ -o $@

bin/bcoro: tests/bcoro/main.c bcoro.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bcoro $^ -o $@

bin/bserial: \
		bserial.h \
		tests/bserial/unstructured.c \
		tests/bserial/array.c \
		tests/bserial/record.c \
		tests/bserial/common.c \
		tests/bserial/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@
