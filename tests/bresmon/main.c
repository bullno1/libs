#define BRESMON_IMPLEMENTATION
#include "../../bresmon.h"
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t should_run = 1;

void int_handler(int dummy) {
    should_run = 0;
}

static inline void
reload(const char* file, void* type) {
	printf("Reloading %s: %s\n", (char*)type, file);
}

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	bresmon_t* mon = bresmon_create(NULL);
	bresmon_watch(mon, "mem_layout.h", reload, "header");
	bresmon_watch(mon, "premake5.lua", reload, "script");

    signal(SIGINT, int_handler);
	while (should_run) {
		int num_events = bresmon_should_reload(mon, true);
		if (num_events > 0) {
			printf("Received %d event(s)\n", num_events);
			int num_reloads = bresmon_reload(mon);
			printf("Reloaded %d resources\n", num_reloads);
		}
	}

	bresmon_destroy(mon);

	return 0;
}
