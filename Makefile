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
	bin/bserial \
	bin/bspscq \
	bin/barray \
	bin/bsv \
	bin/bsfn

clean:
	rm -rf bin
	rm -rf doc

test: bin/test-all bin/bserial bin/bhash bin/bspscq bin/bsfn
	@set -e; \
	for p in $^; do \
		./$$p; \
	done

doc/index.html: Doxyfile
	doxygen

bin/autolist: tests/autolist/a.c tests/autolist/b.c tests/autolist/main.c autolist.h
	mkdir -p bin
	$(CC) $(CFLAGS) $(filter-out %.h, $^) -o $@

bin/xincbin: tests/xincbin/main.c tests/xincbin/resources.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/xincbin $(filter-out %.h, $^) -o $@

bin/mem_layout: tests/mem_layout/main.c mem_layout.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/mem_layout $(filter-out %.h, $^) -o $@

bin/barena: tests/barena/main.c barena.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/barena $(filter-out %.h, $^) -o $@

bin/tlsf: tests/tlsf/main.c tlsf.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/tlsf $(filter-out %.h, $^) -o $@

bin/bresmon: tests/bresmon/main.c bresmon.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bresmon $(filter-out %.h, $^) -o $@

bin/bhash: tests/bhash/main.c bhash.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bhash $(filter-out %.h, $^) -o $@

bin/bserial: \
		bserial.h \
		tests/bserial/unstructured.c \
		tests/bserial/array.c \
		tests/bserial/record.c \
		tests/bserial/table.c \
		tests/bserial/stdio.c \
		tests/bserial/common.c \
		tests/bserial/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) $(filter-out %.h, $^) -o $@

bin/bsfn: \
		bsfn.h tests/bsfn/module/module.c \
		tests/bsfn/basic.c tests/bsfn/no_reload.c tests/bsfn/reload.c \
		tests/bsfn/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) -fPIC -shared -DBSFN_MODULE_VARIANT=1 tests/bsfn/module/module.c -o bin/bsfn_module1.so
	$(CC) $(CFLAGS) -fPIC -shared -DBSFN_MODULE_VARIANT=2 tests/bsfn/module/module.c -o bin/bsfn_module2.so
	$(CC) $(CFLAGS) $(filter-out %.h tests/bsfn/module/%, $^) -o $@ -ldl

bin/bspscq: tests/bspscq/main.c bspscq.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/bspscq $(filter-out %.h, $^) -o $@

bin/test-all: \
		barray.h tests/barray/main.c \
		bhamt.h barena.h tests/bhamt/main.c \
		bent.h tests/bent/shared.c tests/bent/component.c tests/bent/system.c tests/bent/reload.c \
		bsv.h tests/bsv/shared.c tests/bsv/basic.c tests/bsv/versioning.c \
		bscn.h tests/bscn/shared.c tests/bscn/basic.c tests/bscn/pos.c tests/bscn/sub.c \
		bco.h tests/bco/shared.c tests/bco/basic.c tests/bco/cleanup.c tests/bco/subcoro.c tests/bco/clone.c \
		tests/main.c
	mkdir -p bin
	$(CC) $(CFLAGS) $(filter-out %.h, $^) -o $@
