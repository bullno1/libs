// vim: set foldmethod=marker foldlevel=0:
#ifndef BCRASH_HANDLER
#define BCRASH_HANDLER

/**
 * @file
 *
 * @brief Crash handler.
 *
 * Set a function to be called before the program crashes.
 * @ref bstacktrace.h can be used to generate a stacktrace of the crash.
 *
 * Supported platforms:
 *
 * * Windows
 * * Linux
 */

#include <stdint.h>
#include <stdbool.h>

#ifndef BCRASH_HANDLER_API
#define BCRASH_HANDLER_API
#endif

/**
 * Information about a crash
 */
typedef struct {
	/**
	 * The error code.
	 *
	 * On Windows, this is the [exception code](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record#members).
	 *
	 * On Linux, this is the [signal number](https://man7.org/linux/man-pages/man7/signal.7.html#Standard%20signals:~:text=Standard%20signals,-Linux).
	 */
	int code;

	/**
	 * Number of frames to skip during tracing.
	 *
	 * These frame belongs to the crash handling system and are not relevant for debugging.
	 *
	 * @see bcrash_handler_should_report_current_frame
	 */
	int num_handler_frames;

	/**
	 * Address of the instruction at the time of crashing.
	 */
	uintptr_t pc;

	/**
	 * If this is a memory access error, this is the address that caused the crash.
	 */
	uintptr_t fault_addr;
} bcrash_info_t;

/**
 * Crash handler callback
 *
 * Example:
 *
 * @snippet samples/bcrash_handler.c bcrash_handler_fn_t
 */
typedef void (*bcrash_handler_fn_t)(bcrash_info_t crash_info);

/**
 * Set a crash handler.
 *
 * This should only be called once, during initialization.
 */
BCRASH_HANDLER_API void
bcrash_handler_set(bcrash_handler_fn_t handler);

/**
 * Check whether the code location at @ref bcrash_info_t.pc should be reported.
 *
 * Differentt platforms invoke the crash handler differently.
 * The stacktrace originating from the crash handler may or may not encompass
 * the crash site.
 * This function helps to smooth over the differences.
 *
 * Example:
 *
 * @snippet samples/bcrash_handler.c bcrash_handler_should_report_current_pc
 *
 * @param info The crash info in the callback
 * @return Whether the crashing location should be reported
 */
BCRASH_HANDLER_API bool
bcrash_handler_should_report_current_pc(bcrash_info_t* info);

/**
 * Check whether the current stackframe inside a stack walker should be reported.
 *
 * The crash handler itself also has a call stack which is not relevant for debugging.
 * Different platforms have different stack depth for crash handling.
 * This function helps to smooth over the differences.
 *
 * This function is designed to be called inside a @ref bstacktrace.h "bstacktrace"'s callback
 *
 * Example:
 *
 * @snippet samples/bcrash_handler.c bcrash_handler_should_report_current_frame
 *
 * @param info The crash info in the callback
 * @param pc PC of the current frame
 * @return Whether the current stackframe should be reported
 */
BCRASH_HANDLER_API bool
bcrash_handler_should_report_current_frame(bcrash_info_t* info, uintptr_t pc);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BCRASH_HANDLER_IMPLEMENTATION)
#define BCRASH_HANDLER_IMPLEMENTATION
#endif

#ifdef BCRASH_HANDLER_IMPLEMENTATION

#include <stddef.h>

static bcrash_handler_fn_t bcrash_handler = NULL;

#if defined(__linux__)
// Linux {{{

#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdlib.h>

static void
bcrash_signal_handler(int sig, siginfo_t* siginfo, void* ucontext) {
	ucontext_t* ctx = (ucontext_t *)ucontext;
	bcrash_info_t crash_info = {
		.code = sig,
#if defined(__x86_64__)
		.pc = (uintptr_t)ctx->uc_mcontext.gregs[16],  // REG_RIP
#elif defined(__aarch64__)
		.pc = (uintptr_t)ctx->uc_mcontext.pc,
#endif
		.fault_addr = (uintptr_t)siginfo->si_addr,
		.num_handler_frames = 3,
	};

	bcrash_handler(crash_info);
}

void
bcrash_handler_set(bcrash_handler_fn_t handler) {
	bcrash_handler = handler;

	struct sigaction sa;
	sa.sa_sigaction = bcrash_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS,  &sa, NULL);
	sigaction(SIGILL,  &sa, NULL);
	sigaction(SIGFPE,  &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
}

bool
bcrash_handler_should_report_current_pc(bcrash_info_t* info) {
	return true;
}

bool
bcrash_handler_should_report_current_frame(bcrash_info_t* info, uintptr_t pc) {
	if (info->num_handler_frames > 0) {
		info->num_handler_frames -= 1;
		return false;
	} else {
		return true;
	}
}

// }}}
#elif defined(_WIN32)
// Windows {{{

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static
LONG WINAPI bcrash_handler_veh(EXCEPTION_POINTERS* ep) {
	bcrash_info_t crash_info = {
		.code = ep->ExceptionRecord->ExceptionCode,
#if defined(_M_X64)
		.pc = (uintptr_t)ep->ContextRecord->Rip,
#elif defined(_M_IX86)
		.pc = (uintptr_t)ep->ContextRecord->Eip,
#elif defined(_M_ARM64)
		.pc = (uintptr_t)ep->ContextRecord->Pc,
#endif
		.fault_addr = (uintptr_t)ep->ExceptionRecord->ExceptionInformation[1],
		.num_handler_frames = 5,
	};

	bcrash_handler(crash_info);
	return EXCEPTION_CONTINUE_SEARCH;
}

void
bcrash_handler_set(bcrash_handler_fn_t handler) {
	bcrash_handler = handler;

	AddVectoredExceptionHandler(1, bcrash_handler_veh);
}

bool
bcrash_handler_should_report_current_pc(bcrash_info_t* info) {
	return false;
}

bool
bcrash_handler_should_report_current_frame(bcrash_info_t* info, uintptr_t pc) {
	// We have reached user code
	if (info->num_handler_frames == 0) { return true; }

	// Skip until we are back to the crashing code
	if (info->pc != pc) {
		return false;
	} else {
		info->num_handler_frames = 0;
		return true;
	}
}

// }}}
#else

#error "Unsupported platform"

#endif

#endif
