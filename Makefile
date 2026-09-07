.PHONY: all clean test

all: doc/index.html bin/tests

clean:
	rm -rf bin
	rm -rf doc

test: bin/tests
	./bin/tests

doc/index.html: Doxyfile README.md $(wildcard *.h)
	doxygen

# bsfn only supports Linux: the test dlopens two builds of the same module
bin/bsfn_module1.so: tests/bsfn/module/module.c bsfn.h
	mkdir -p bin
	$(CC) $(CFLAGS) -fPIC -shared -DBSFN_MODULE_VARIANT=1 $< -o $@

bin/bsfn_module2.so: tests/bsfn/module/module.c bsfn.h
	mkdir -p bin
	$(CC) $(CFLAGS) -fPIC -shared -DBSFN_MODULE_VARIANT=2 $< -o $@

# Every directory under tests/ is a btest suite linked into a single binary
TEST_SOURCES := tests/main.c $(wildcard tests/*/*.c)

bin/tests: $(TEST_SOURCES) $(wildcard *.h) $(wildcard tests/*/*.h) bin/bsfn_module1.so bin/bsfn_module2.so
	mkdir -p bin
	$(CC) $(CFLAGS) -Itests/xincbin $(TEST_SOURCES) -o $@ -ldl
