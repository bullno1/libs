#define BARENA_IMPLEMENTATION
#include "../../barena.h"
#include <stdio.h>
#include <string.h>

static inline void
prompt(const char* line) {
	printf("%s\n", line);
	getc(stdin);
}

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	barena_pool_t pool;
	barena_pool_init(&pool, 4096ull * 2);

	barena_t arena;
	barena_init(&arena, &pool);

	barena_snapshot_t snapshot = barena_snapshot(&arena);
	prompt("Allocating 1MiB");
	void* mem = barena_malloc(&arena, 1024ull * 1024);
	memset(mem, 0, 1024ull * 1024);
	printf("mem = %p\n", mem);

	prompt("Allocating 2MiB");
	mem = barena_malloc(&arena, 2048ull * 1024);
	memset(mem, 0, 1024ull * 1024);
	printf("mem = %p\n", mem);

	prompt("Restoring snapshot");
	barena_restore(&arena, snapshot);

	prompt("Allocating 1MiB");
	mem = barena_malloc(&arena, 1024ull * 1024);
	printf("mem = %p\n", mem);

	prompt("Allocating 2MiB");
	mem = barena_malloc(&arena, 2048ull * 1024);
	printf("mem = %p\n", mem);

	prompt("Allocating 2MiB");
	mem = barena_malloc(&arena, 2048ull * 1024);
	printf("mem = %p\n", mem);

	prompt("Cleaning up");
	barena_reset(&arena);

	barena_pool_cleanup(&pool);

	prompt("Exiting");
	return 0;
}
