bin/reglist: tests/reglist/a.c tests/reglist/b.c tests/reglist/main.c
	mkdir -p bin
	$(CC) $^ -o $@
