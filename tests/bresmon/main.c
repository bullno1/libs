#define BRESMON_IMPLEMENTATION
#include "../../bresmon.h"
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t should_run = 1;

#if defined(__linux__)

static void int_handler(int dummy) {
    should_run = 0;
}

static void setup_ctrlc_handler(void) {
	signal(SIGINT, int_handler);
}

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

static HANDLE main_thread = INVALID_HANDLE_VALUE;

static void WINAPI dummy_apc(ULONG_PTR ctx) {
	(void)ctx;
}

static BOOL WINAPI ctrl_handler(DWORD fdwCtrlType) {
	should_run = 0;
	QueueUserAPC(dummy_apc, main_thread, (ULONG_PTR)NULL);
	CloseHandle(main_thread);
	return TRUE;
}

static void setup_ctrlc_handler(void) {
	main_thread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
	SetConsoleCtrlHandler(ctrl_handler, TRUE);
}

#endif

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

	setup_ctrlc_handler();
	while (should_run) {
		int num_events = bresmon_should_reload(mon, true);
		if (num_events > 0) {
			printf("Received %d event(s)\n", num_events);
			int num_reloads = bresmon_reload(mon);
			printf("Reloaded %d resources\n", num_reloads);
		}
	}

	printf("Cleaning up\n");
	bresmon_destroy(mon);

	return 0;
}
