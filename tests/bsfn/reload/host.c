#define BSFN_IMPLEMENTATION
#include "../../../bsfn.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>

typedef int (*version_fn_t)(void);
typedef version_fn_t (*entry_fn_t)(bsfn_ctx_t*);

#define CHECK(COND) \
	do { \
		if (!(COND)) { \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #COND); \
			return 1; \
		} \
	} while (0)

static entry_fn_t
load_module(void** lib, const char* path) {
	*lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (*lib == NULL) {
		fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
		return NULL;
	}
	// Function pointers must be laundered through an integer to please ISO C
	return (entry_fn_t)(uintptr_t)dlsym(*lib, "module_entry");
}

int
main(int argc, const char* argv[]) {
	const char* bin_dir = argc > 1 ? argv[1] : "bin";
	char path[256];
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	CHECK(ctx != NULL);

	void* lib = NULL;
	snprintf(path, sizeof(path), "%s/bsfn_module1.so", bin_dir);
	entry_fn_t entry = load_module(&lib, path);
	CHECK(entry != NULL);
	version_fn_t stable = entry(ctx);
	CHECK(stable() == 1);
	CHECK(dlclose(lib) == 0);

	snprintf(path, sizeof(path), "%s/bsfn_module2.so", bin_dir);
	entry = load_module(&lib, path);
	CHECK(entry != NULL);
	version_fn_t stable2 = entry(ctx);

	// Same stable pointer, new behavior
	CHECK(stable2 == stable);
	CHECK(stable() == 2);

	// A permanently unloaded module can detach its stubs so a stale call
	// traps instead of jumping into unmapped memory (not exercised here as
	// it would abort the test).
	CHECK(dlclose(lib) == 0);

	bsfn_ctx_destroy(ctx);
	printf("OK\n");
	return 0;
}
