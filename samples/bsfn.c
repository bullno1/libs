#include "../bsfn.h"
#include "../bresmon.h"

// Note: Ignore the `//! [label]` markers, they are for for Doxygen
// They are pushed out of the way so you don't need to pay attention to them

// This sample sketches a host and a hot-reloadable module in one file.
// In a real program the module part lives in a shared library, reloaded with
// something like remodule (https://github.com/bullno1/remodule).

typedef struct {
	bsfn_ctx_t* bsfn;
	bresmon_t* mon;
	bresmon_watch_t* config_watch;
} host_state_t;

//!                                                                             [bsfn_module]
static void
on_config_changed(const char* file, void* userdata) {
	(void)file;
	(void)userdata;
	// Reload the config...
}

// Called by the host on every load, including the first one
void
module_entry(host_state_t* host, bool first_load) {
	// Create or repoint the stubs of all BSFN-wrapped functions in this module
	bsfn_bind(host->bsfn);

	if (first_load) {
		// The callback is registered only once: BSFN yields a pointer that
		// stays valid across reloads, no re-registration idiom
		// (e.g bresmon_init_watch) is needed
		host->config_watch = bresmon_watch(
			host->mon, "config.ini", BSFN(on_config_changed), host
		);
	}
}
//!                                                                             [bsfn_module]

//!                                                                             [bsfn_host]
int
main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	host_state_t host = {
		// Created once by the host, before any module is loaded
		.bsfn = bsfn_ctx_create(NULL),
		.mon = bresmon_create(NULL),
	};

	// Load the module and call its entry point.
	// On every subsequent reload the entry point is called again and
	// bsfn_bind repoints the stubs at the new code.
	module_entry(&host, true);

	// Main loop
	// while (running) { bresmon_check(host.mon, false); ... }

	bresmon_destroy(host.mon);
	bsfn_ctx_destroy(host.bsfn);
	return 0;
}
//!                                                                             [bsfn_host]

#define BLIB_IMPLEMENTATION
#include "../bsfn.h"
#include "../bresmon.h"
