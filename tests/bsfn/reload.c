// An actual dlopen/dlclose reload: the same module source is built into two
// shared libraries (v1 and v2, see module/module.c) and the stable pointer
// obtained from v1 must reach the v2 code after a reload.
#include "../../bsfn.h"
#include "../../btest.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static btest_suite_t bsfn_dl = {
	.name = "bsfn_reload",
};

typedef int (*version_fn_t)(void);
typedef version_fn_t (*entry_fn_t)(bsfn_ctx_t*);

// The modules sit next to the test executable
static bool
module_path(char* buf, size_t size, int variant) {
	char exe[512];
	ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (len < 0) { return false; }
	exe[len] = '\0';
	char* slash = strrchr(exe, '/');
	if (slash == NULL) { return false; }
	*slash = '\0';
	int written = snprintf(buf, size, "%s/bsfn_module%d.so", exe, variant);
	return written > 0 && (size_t)written < size;
}

static entry_fn_t
load_module(void** lib, int variant) {
	char path[512];
	if (!module_path(path, sizeof(path), variant)) { return NULL; }
	*lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (*lib == NULL) {
		fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
		return NULL;
	}
	// Function pointers must be laundered through an integer to please ISO C
	return (entry_fn_t)(uintptr_t)dlsym(*lib, "module_entry");
}

BTEST(bsfn_dl, stable_across_dlopen) {
	bsfn_ctx_t* ctx = bsfn_ctx_create(NULL);
	BTEST_ASSERT(ctx != NULL);

	void* lib = NULL;
	entry_fn_t entry = load_module(&lib, 1);
	BTEST_ASSERT(entry != NULL);
	version_fn_t stable = entry(ctx);
	BTEST_ASSERT_EQUAL("%d", stable(), 1);
	BTEST_ASSERT_EQUAL("%d", dlclose(lib), 0);

	entry = load_module(&lib, 2);
	BTEST_ASSERT(entry != NULL);
	version_fn_t stable2 = entry(ctx);

	// Same stable pointer, new behavior
	BTEST_ASSERT(stable2 == stable);
	BTEST_ASSERT_EQUAL("%d", stable(), 2);

	// A permanently unloaded module can detach its stubs so a stale call
	// traps instead of jumping into unmapped memory (not exercised here as
	// it would abort the test).
	BTEST_ASSERT_EQUAL("%d", dlclose(lib), 0);

	bsfn_ctx_destroy(ctx);
}
