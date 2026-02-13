// vim: set foldmethod=marker foldlevel=0:
#ifndef BSTACKTRACE_H
#define BSTACKTRACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @file
 *
 * @brief Portable stacktrace with source mapping.
 *
 * Supported platforms:
 *
 * * Windows
 * * Linux
 * * Emscripten
 *
 * On all platforms, the executable should be compiled with debug info enabled.
 *
 * On Windows [dbghelp](https://learn.microsoft.com/en-us/windows/win32/debug/debug-help-library) is used.
 * With MSVC, it will be automatically linked.
 *
 * On Linux, [libdw](https://sourceware.org/elfutils/) is an optional dependency at both compile and runtime.
 * Without it, source level information cannot be resolved.
 * It is recommended to compile with `-fno-omit-frame-pointer`.
 * When using the [mold](https://github.com/rui314/mold) linker, `--separate-debug-file` will result in an **incompatible** debug info file.
 *
 * On Emscripten, the flags `-sASYNCIFY=1 -gsource-map` are required for debug info resolution to work.
 */

#ifndef BSTACKTRACE_API
#define BSTACKTRACE_API
#endif


/**
 * Resolve everything about the stackframe.
 *
 * @see bstacktrace_resolve
 * @see bstacktrace_resolve_flag_t
 */
#define BSTACKTRACE_RESOLVE_ALL  ((bstacktrace_resolve_flags_t)0xffffffff)

/**
 * A stacktracer context
 *
 * @see bstacktrace_init
 * @see bstacktrace_cleanup
 */
typedef struct bstacktrace_s bstacktrace_t;

/**
 * Information about a stackframe
 *
 * @see bstacktrace_resolve
 */
typedef struct {
	/**
	 * Name of the module this stackframe belongs to, enabled with @ref BSTACKTRACE_RESOLVE_MODULE.
	 *
	 * Even when enabled, this may still be `NULL` due to a lack of debug info.
	 *
	 * On Emscripten, this will always be `NULL` for Javascript code.
	 *
	 * @remarks This string is temporary and should be immediately copied before further calls to @ref bstacktrace_resolve.
	 */
	const char* module;

	/**
	 * Name of the function this stackframe belongs to, enabled with @ref BSTACKTRACE_RESOLVE_FUNCTION.
	 *
	 * Even when enabled, this may still be `NULL` due to a lack of debug info.
	 *
	 * @remarks This string is temporary and should be immediately copied before further calls to @ref bstacktrace_resolve.
	 */
	const char* function;

	/**
	 * Name of the file this stackframe belongs to, enabled with @ref BSTACKTRACE_RESOLVE_FILENAME.
	 *
	 * Even when enabled, this may still be `NULL` due to a lack of debug info.
	 *
	 * @remarks This string is temporary and should be immediately copied before further calls to @ref bstacktrace_resolve.
	 */
	const char* filename;

	/**
	 * The line number of this stackframe, enabled with @ref BSTACKTRACE_RESOLVE_LINE.
	 *
	 * Even when enabled, this may still be 0 due to a lack of debug info.
	 */
	int line;

	/**
	 * The column number of this stackframe, enabled with @ref BSTACKTRACE_RESOLVE_COLUMN.
	 *
	 * Even when enabled, this may still be `0` due to a lack of debug info.
	 *
	 * On Windows, his will always be `0`.
	 */
	int column;
} bstacktrace_info_t;

/**
 * Flag values for debug symbol resolution.
 *
 * @see bstacktrace_resolve
 */
typedef enum {
	/*! Resolve @ref bstacktrace_info_t.module */
	BSTACKTRACE_RESOLVE_MODULE    = 1 << 0,
	/*! Resolve @ref bstacktrace_info_t.function */
	BSTACKTRACE_RESOLVE_FUNCTION  = 1 << 1,
	/*! Resolve @ref bstacktrace_info_t.filename */
	BSTACKTRACE_RESOLVE_FILENAME  = 1 << 2,
	/*! Resolve @ref bstacktrace_info_t.line */
	BSTACKTRACE_RESOLVE_LINE      = 1 << 3,
	/*! Resolve @ref bstacktrace_info_t.column */
	BSTACKTRACE_RESOLVE_COLUMN    = 1 << 4,
} bstacktrace_resolve_flag_t;

typedef int bstacktrace_resolve_flags_t;

/**
 * Stack walk callback.
 *
 * This will be invoked on successive stackframes, starting with the caller of @ref bstacktrace_walk.
 *
 * Example:
 *
 * @snippet samples/bstacktrace.c bstacktrace_callback_fn_t
 *
 * @param address Address of the current stackframe.
 *   This should be passed to @ref bstacktrace_resolve for further info.
 * @param userdata Arbitrary datra passed to @ref bstacktrace_walk
 * @return Whether The walk should continue. Return `false` to terminate early.
 *
 * @see bstacktrace_walk
 */
typedef bool (*bstacktrace_callback_fn_t)(uintptr_t address, void* userdata);

/**
 * Initialize a new stacktracer.
 *
 * @param memctx See @ref allocator
 * @return A new stacktracer
 *
 * @see bstacktrace_cleanup
 */
BSTACKTRACE_API bstacktrace_t*
bstacktrace_init(void* memctx);

/**
 * Cleanup an existing stacktracer.
 *
 * @param ctx A stacktracer context
 *
 * @see bstacktrace_init
 */
BSTACKTRACE_API void
bstacktrace_cleanup(bstacktrace_t* ctx);

/**
 * Walk the stack, starting from the caller of this function.
 *
 * @param ctx A stacktracer context
 * @param callback A stack walk callback
 * @param userdata Arbitrary userdata to pass to the callback
 *
 * @see bstacktrace_init
 * @see bstacktrace_callback_fn_t
 */
BSTACKTRACE_API void
bstacktrace_walk(
	bstacktrace_t* ctx,
	bstacktrace_callback_fn_t callback,
	void* userdata
);

/**
 * Resolve informations about a stackframe.
 *
 * @param ctx A stacktracer context
 * @param address An address passed to a @ref bstacktrace_callback_fn_t
 * @param flags Which fields of @ref bstacktrace_info_t should be resolved.
 * @return Information about the stackframe
 */
BSTACKTRACE_API bstacktrace_info_t
bstacktrace_resolve(bstacktrace_t* ctx, uintptr_t address, bstacktrace_resolve_flags_t flags);

/**
 * Refresh the internal state of a stacktracer.
 *
 * On some platforms, locating and loading symbols is an expensive operation.
 * @ref bstacktrace_init does this once for the program and all its loaded modules.
 * However, with dynamnic loading, the loaded symbols may be out-of-date.
 *
 * This function should be called after the program loads or unloads a dynamic library at runtime (e.g: using `LoadLibrary` on Windows or `dlopen` on Unix platforms).
 *
 * @param ctx A stacktracer context
 */
BSTACKTRACE_API void
bstacktrace_refresh(bstacktrace_t* ctx);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSTACKTRACE_IMPLEMENTATION)
#define BSTACKTRACE_IMPLEMENTATION
#endif

#ifdef BSTACKTRACE_IMPLEMENTATION

// Common {{{

#ifndef BSTACKTRACE_REALLOC
#	ifdef BLIB_REALLOC
#		define BSTACKTRACE_REALLOC BLIB_REALLOC
#	else
#		define BSTACKTRACE_REALLOC(ptr, size, ctx) bstacktrace__libc_realloc(ptr, size, ctx)
#		define BSTACKTRACE_USE_LIBC
#	endif
#endif

#ifdef BSTACKTRACE_USE_LIBC

#include <stdlib.h>

static inline void*
bstacktrace__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

#if defined(_MSC_VER)
#	define BSTACKTRACE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#	define BSTACKTRACE_NOINLINE __attribute__((noinline))
#else
#	define BSTACKTRACE_NOINLINE
#endif

// }}}

#if defined(__EMSCRIPTEN__)
// emscripten {{{

#include <string.h>
#include <emscripten.h>

const char* emscripten_pc_get_function(uintptr_t pc);
const char* emscripten_pc_get_file(uintptr_t pc);
int emscripten_pc_get_line(uintptr_t pc);
int emscripten_pc_get_column(uintptr_t pc);
uintptr_t emscripten_stack_snapshot(void);
uint32_t emscripten_stack_unwind_buffer(uintptr_t pc, uintptr_t* buffer, uint32_t depth);

EM_ASYNC_JS(void, bstacktrace_init_source_map_support, (), {
  class WasmSourceMap {
    mapping = {};
    offsets = [];

    constructor(sourceMap) {
      this.version = sourceMap.version;
      this.sources = sourceMap.sources;
      this.names = sourceMap.names;

      var vlqMap = {};
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=".split("").forEach((c, i) => vlqMap[c] = i);

      function decodeVLQ(string) {
        var result = [];
        var shift = 0;
        var value = 0;

        for (var ch of string) {
          var integer = vlqMap[ch];
          if (integer === undefined) {
            throw new Error(`Invalid character (${ch})`);
          }

          value += (integer & 31) << shift;

          if (integer & 32) {
            shift += 5;
          } else {
            var negate = value & 1;
            value >>= 1;
            result.push(negate ? -value : value);
            value = shift = 0;
          }
        }
        return result;
      }

      var offset = 0, src = 0, line = 1, col = 1, name = 0;
      for (const [index, segment] of sourceMap.mappings.split(',').entries()) {
        if (!segment) continue;
        var data = decodeVLQ(segment);
        var info = {};

        offset += data[0];
        if (data.length >= 2) info.source = src += data[1];
        if (data.length >= 3) info.line = line += data[2];
        if (data.length >= 4) info.column = col += data[3];
        if (data.length >= 5) info.name = name += data[4];
        this.mapping[offset] = info;
        this.offsets.push(offset);
      }
      this.offsets.sort((a, b) => a - b);
    }

    lookup(offset) {
      var normalized = this.normalizeOffset(offset);
      var info = this.mapping[normalized];
      if (!info) {
        return null;
      }
      return {
        file: this.sources[info.source],
        line: info.line,
        column: info.column,
        name: this.names[info.name],
      };
    }

    normalizeOffset(offset) {
      var lo = 0;
      var hi = this.offsets.length;
      var mid;

      while (lo < hi) {
        mid = Math.floor((lo + hi) / 2);
        if (this.offsets[mid] > offset) {
          hi = mid;
        } else {
          lo = mid + 1;
        }
      }
      return this.offsets[lo - 1];
    }
  }

  const wasmSourceMapFile = findWasmBinary() + '.map';
  bstacktrace_source_map = null;

  try {
    const response = await fetch(wasmSourceMapFile, { credentials: 'same-origin' });
    const json = await response.json();
    bstacktrace_source_map = new WasmSourceMap(json);
  } catch (e) {
    console.error(e);
  }
});

struct bstacktrace_s {
	void* memctx;
	char* last_filename;
	char* last_function;
	char* last_module;
} bstacktrace_dummy;

bstacktrace_t*
bstacktrace_init(void* memctx) {
	bstacktrace_init_source_map_support();
	bstacktrace_t* ctx = BSTACKTRACE_REALLOC(NULL, sizeof(bstacktrace_t), memctx);
	*ctx = (bstacktrace_t){
		.memctx = memctx,
	};
	return ctx;
}

void
bstacktrace_cleanup(bstacktrace_t* ctx) {
	free(ctx->last_filename);
	free(ctx->last_function);
	free(ctx->last_module);
	BSTACKTRACE_REALLOC(ctx, 0, ctx->memctx);
}

void
bstacktrace_reload(bstacktrace_t* ctx) {
}

BSTACKTRACE_NOINLINE void
bstacktrace_walk(
	bstacktrace_t* ctx,
	bstacktrace_callback_fn_t callback,
	void* userdata
) {
	uintptr_t pcs[64];

	uintptr_t snapshot = emscripten_stack_snapshot();
	int count = emscripten_stack_unwind_buffer(snapshot, pcs, 64);
	for (int i = 1; i < count; ++i) {
		if (!callback(pcs[i], userdata)) {
			break;
		}
	}
}

EM_JS_DEPS(bstacktrace_resolve_source, "$stringToNewUTF8,$UNWIND_CACHE,$setValue");
EM_JS(void, bstacktrace_resolve_source, (uintptr_t pc, char** filename, int* line, int* column), {
	let source = null;
	if (bstacktrace_source_map) {
		source = bstacktrace_source_map.lookup(pc);
	}

	if (!source) {
		const frame = UNWIND_CACHE[pc];
		if (frame) {
			if (match = /\\((.*):(\\d+):(\\d+)\\)$/.exec(frame)) {
				source = {file: match[1], line: match[2], column: match[3]};
			} else if (match = /@(.*):(\\d+):(\\d+)/.exec(frame)) {
				source = {file: match[1], line: match[2], column: match[3]};
			}
		}
	}

	if (source) {
		setValue(line, source.line || 0, 'i32');
		setValue(column, source.column || 0, 'i32');
		setValue(filename, stringToNewUTF8(source.file), 'i32');
	}
});

EM_JS_DEPS(bstacktrace_resolve_function, "$stringToNewUTF8,$UNWIND_CACHE");
EM_JS(char*, bstacktrace_resolve_function, (uintptr_t pc), {
	const frame = UNWIND_CACHE[pc];
	if (!frame) return 0;

	let name;
	let match;
	if (match = /^\\s+at .*\\.wasm\\.(.*) \\(.*\\)$/.exec(frame)) {
		name = match[1];
	} else if (match = /^\\s+at (.*) \\(.*\\)$/.exec(frame)) {
		name = match[1];
	} else if (match = /^.*\\.wasm\\.(.*)@/.exec(frame)) {
		name = match[1];
	} else if (match = /^(.+?)@/.exec(frame)) {
		name = match[1];
	} else {
		return 0;
	}

	return stringToNewUTF8(name);
});

EM_JS_DEPS(bstacktrace_resolve_module, "$stringToNewUTF8,$UNWIND_CACHE");
EM_JS(char*, bstacktrace_resolve_module, (uintptr_t pc), {
	const frame = UNWIND_CACHE[pc];
	if (!frame) return 0;

	let name;
	let match;
	if (match = /^\\s+at (.*)\\.wasm\\..* \\(.*\\)$/.exec(frame)) {
		name = match[1];
	} else if (match = /^(.*)\\.wasm\\..*@/.exec(frame)) {
		name = match[1];
	} else {
		return 0;
	}

	return stringToNewUTF8(name);
});

bstacktrace_info_t
bstacktrace_resolve(bstacktrace_t* ctx, uintptr_t address, bstacktrace_resolve_flags_t flags) {
	bstacktrace_info_t info = { 0 };

	if (flags & BSTACKTRACE_RESOLVE_FUNCTION) {
		free(ctx->last_function);
		info.function = ctx->last_function = bstacktrace_resolve_function(address);
	}

	if (flags & BSTACKTRACE_RESOLVE_MODULE) {
		free(ctx->last_module);
		info.module = ctx->last_module = bstacktrace_resolve_module(address);
	}

	if (flags & (BSTACKTRACE_RESOLVE_FILENAME | BSTACKTRACE_RESOLVE_LINE | BSTACKTRACE_RESOLVE_COLUMN)) {
		free(ctx->last_filename);
		bstacktrace_resolve_source(address, &ctx->last_filename, &info.line, &info.column);
		info.filename = ctx->last_filename;
	}

	return info;
}

// }}}
#elif defined(__linux__)
// Linux {{{

#include <string.h>

// libdw {{{

#ifdef __has_include
#	if __has_include(<elfutils/libdwfl.h>)
#		define BSTACKTRACE_HAS_LIBDWFL
#	endif
#endif

#ifdef BSTACKTRACE_HAS_LIBDWFL

#include <dlfcn.h>
#include <elfutils/libdwfl.h>
#include <unistd.h>

#define BSTACKTRACE_LIBDWFL_FN(X) \
	X(dwfl_begin) \
	X(dwfl_report_begin) \
	X(dwfl_report_end) \
	X(dwfl_linux_proc_report) \
	X(dwfl_end) \
	X(dwfl_addrmodule) \
	X(dwfl_module_info) \
	X(dwfl_module_addrname) \
	X(dwfl_module_getsrc) \
	X(dwfl_lineinfo) \
	X(dwfl_linux_proc_find_elf) \
	X(dwfl_standard_find_debuginfo) \
	X(dwfl_offline_section_address) \


#if __STDC_VERSION__ >= 202311L
#	define BSTACKTRACE_TYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BSTACKTRACE_TYPEOF(EXP) __typeof__(EXP)
#endif

#define BSTACKTRACE_DECLARE_FN_PTR(NAME) BSTACKTRACE_TYPEOF(&NAME) NAME;

typedef struct {
    void* handle;

	Dwfl_Callbacks callbacks;

	BSTACKTRACE_LIBDWFL_FN(BSTACKTRACE_DECLARE_FN_PTR)
} bstacktrace_libdw_t;

typedef Dwfl bstacktrace_libdw_session_t;

static void
bstacktrace_libdw_load(bstacktrace_libdw_t* lib) {
    lib->handle = dlopen("libdw.so.1", RTLD_NOW | RTLD_LOCAL);
    if (lib->handle == NULL) {
        lib->handle = dlopen("libdw.so", RTLD_NOW | RTLD_LOCAL);
	}

    if (lib->handle == NULL) {
		return;
    }

#define BSTACKTRACE_LOAD_SYM(NAME) lib->NAME = (BSTACKTRACE_TYPEOF(lib->NAME))dlsym(lib->handle, #NAME);

	BSTACKTRACE_LIBDWFL_FN(BSTACKTRACE_LOAD_SYM)

	lib->callbacks.find_elf = lib->dwfl_linux_proc_find_elf;
	lib->callbacks.find_debuginfo = lib->dwfl_standard_find_debuginfo;
	lib->callbacks.section_address = lib->dwfl_offline_section_address;
}

static void
bstacktrace_libdw_unload(bstacktrace_libdw_t* lib) {
	dlclose(lib->handle);
}

static void
bstacktrace_libdw_session_begin(bstacktrace_libdw_t* lib, bstacktrace_libdw_session_t** session_ptr) {
	*session_ptr = NULL;

	if (lib->handle == NULL) { return; }

	bstacktrace_libdw_session_t* session = lib->dwfl_begin(&lib->callbacks);
	if (session == NULL) { return; }

	lib->dwfl_report_begin(session);
	if (lib->dwfl_linux_proc_report(session, getpid()) != 0) { return; }
	if (lib->dwfl_report_end(session, NULL, NULL) != 0) { return; }

	*session_ptr = session;
}

static void
bstacktrace_libdw_session_end(bstacktrace_libdw_t* lib, bstacktrace_libdw_session_t** session_ptr) {
	bstacktrace_libdw_session_t* session = *session_ptr;
	if (session == NULL) { return; }

	lib->dwfl_end(session);
	*session_ptr = NULL;
}

static void
bstacktrace_libdw_resolve(
	bstacktrace_libdw_t* lib,
	bstacktrace_libdw_session_t* session,
	uintptr_t pc,
	bstacktrace_resolve_flags_t flags,
	bstacktrace_info_t* info
) {
	if (lib->handle == NULL) { return; }
	if (session == NULL) { return; }

    Dwfl_Module* mod = lib->dwfl_addrmodule(session, pc);
	if (mod == NULL) { return; }

	if (flags & BSTACKTRACE_RESOLVE_MODULE) {
		info->module = lib->dwfl_module_info(mod, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	}

	if (flags & BSTACKTRACE_RESOLVE_FUNCTION) {
		info->function = lib->dwfl_module_addrname(mod, pc);
	}

	if (flags & (BSTACKTRACE_RESOLVE_FILENAME | BSTACKTRACE_RESOLVE_LINE | BSTACKTRACE_RESOLVE_COLUMN)) {
		Dwfl_Line *line = lib->dwfl_module_getsrc(mod, pc);
		if (line != NULL) {
			Dwarf_Addr addr;
			info->filename = lib->dwfl_lineinfo(
				line, &addr,
				&info->line,
				&info->column,
				NULL, NULL
			);
		}
	}
}

#else

#warning "libdwfl not found. Stacktrace will not contain source information"

typedef struct {
	char dummy;
} bstacktrace_libdw_t;

typedef void bstacktrace_libdw_session_t;

static void
bstacktrace_libdw_load(bstacktrace_libdw_t* lib) {
}

static void
bstacktrace_libdw_unload(bstacktrace_libdw_t* lib) {
}

static void
bstacktrace_libdw_session_begin(bstacktrace_libdw_t* lib, bstacktrace_libdw_session_t** session_ptr) {
}

static void
bstacktrace_libdw_session_end(bstacktrace_libdw_t* lib, bstacktrace_libdw_session_t** session_ptr) {
}

static void
bstacktrace_libdw_resolve(
	bstacktrace_libdw_t* lib,
	bstacktrace_libdw_session_t* session,
	uintptr_t pc,
	bstacktrace_resolve_flags_t flags,
	bstacktrace_entry_t* entry
) {
}

#endif

// }}}

struct bstacktrace_s {
	void* memctx;
	bstacktrace_libdw_t libdw;
	bstacktrace_libdw_session_t* libdw_session;
};

bstacktrace_t*
bstacktrace_init(void* memctx) {
	bstacktrace_t* ctx = BSTACKTRACE_REALLOC(NULL, sizeof(bstacktrace_t), memctx);
	*ctx = (bstacktrace_t){
		.memctx = memctx,
	};
	bstacktrace_libdw_load(&ctx->libdw);
	bstacktrace_libdw_session_begin(&ctx->libdw, &ctx->libdw_session);
	return ctx;
}

void
bstacktrace_cleanup(bstacktrace_t* ctx) {
	bstacktrace_libdw_session_end(&ctx->libdw, &ctx->libdw_session);
	bstacktrace_libdw_unload(&ctx->libdw);
	BSTACKTRACE_REALLOC(ctx, 0, ctx->memctx);
}

void
bstacktrace_reload(bstacktrace_t* ctx) {
	bstacktrace_libdw_session_end(&ctx->libdw, &ctx->libdw_session);
	bstacktrace_libdw_session_begin(&ctx->libdw, &ctx->libdw_session);
}

#define BSTACKTRACE_DO(X) \
	X(0) \
	X(1) \
	X(2) \
	X(3) \
	X(4) \
	X(5) \
	X(6) \
	X(7) \
	X(8) \
	X(9) \
	X(10) \
	X(11) \
	X(12) \
	X(13) \
	X(14) \
	X(15) \
	X(16) \
	X(17) \
	X(18) \
	X(19) \
	X(20) \
	X(21) \
	X(22) \
	X(23) \
	X(24) \
	X(25) \
	X(26) \
	X(27) \
	X(28) \
	X(29) \
	X(30) \
	X(31)

#define BSTACKTRACE_WALK(N) \
	do { \
		if (__builtin_frame_address(N) != NULL && __builtin_return_address(N) != NULL) { \
			if (!callback((uintptr_t)__builtin_return_address(N), userdata)) { \
				goto end_walk; \
			}; \
		} else { \
			goto end_walk; \
		} \
	} while (0);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"

BSTACKTRACE_NOINLINE void
bstacktrace_walk(
	bstacktrace_t* ctx,
	bstacktrace_callback_fn_t callback,
	void* userdata
) {
	BSTACKTRACE_DO(BSTACKTRACE_WALK)
end_walk:;
}

#pragma GCC diagnostic pop

bstacktrace_info_t
bstacktrace_resolve(bstacktrace_t* ctx, uintptr_t address, bstacktrace_resolve_flags_t flags) {
	bstacktrace_info_t info = { 0 };
	bstacktrace_libdw_resolve(&ctx->libdw, ctx->libdw_session, address, flags, &info);
	return info;
}

// }}}
#elif defined(_WIN32)
// Windows {{{

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <dbghelp.h>
#include <string.h>

#if defined(_MSC_VER)
#	pragma comment(lib, "dbghelp.lib")
#endif

struct bstacktrace_s {
	void* memctx;
	HANDLE process;

	_Alignas(SYMBOL_INFO) char symbol_info[sizeof(SYMBOL_INFO) + 256];
	IMAGEHLP_MODULE64 module;
	IMAGEHLP_LINE64 line;
};

bstacktrace_t*
bstacktrace_init(void* memctx) {
	bstacktrace_t* ctx = BSTACKTRACE_REALLOC(
		NULL,
		sizeof(bstacktrace_t),
		memctx
	);

	*ctx = (bstacktrace_t){
		.memctx = memctx,
		.process = GetCurrentProcess(),
		.module = { .SizeOfStruct = sizeof(IMAGEHLP_MODULE64) },
		.line = { .SizeOfStruct = sizeof(IMAGEHLP_LINE64) },
	};

	SYMBOL_INFO* symbol_info = (SYMBOL_INFO*)ctx->symbol_info;
	symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol_info->MaxNameLen = 255;

	SymInitialize(ctx->process, NULL, TRUE);
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

	return ctx;
}

void
bstacktrace_cleanup(bstacktrace_t* ctx) {
	SymCleanup(ctx->process);
	BSTACKTRACE_REALLOC(ctx, 0, ctx->memctx);
}

void
bstacktrace_refresh(bstacktrace_t* ctx) {
	SymRefreshModuleList(ctx->process);
}

BSTACKTRACE_NOINLINE void
bstacktrace_walk(
	bstacktrace_t* ctx,
	bstacktrace_callback_fn_t callback,
	void* userdata
) {
	void* frames[32];
	USHORT captured = CaptureStackBackTrace(
		1,  // Skip this function
		32,
		frames,
		NULL
	);
	for (USHORT i = 0; i < captured; ++i) {
		if (!callback((uintptr_t)frames[i], userdata)) {
			break;
		}
	}
}

bstacktrace_info_t
bstacktrace_resolve(bstacktrace_t* ctx, uintptr_t address, bstacktrace_resolve_flags_t flags) {
	bstacktrace_info_t info = { 0 };

	if (flags & BSTACKTRACE_RESOLVE_FUNCTION) {
		DWORD64 displacement = 0;
		SYMBOL_INFO* sym = (SYMBOL_INFO*)&ctx->symbol_info;
		if (SymFromAddr(ctx->process, (DWORD64)address, &displacement, sym)) {
			info.function = sym->Name;
		}
	}

	if (flags & BSTACKTRACE_RESOLVE_MODULE) {
		if (SymGetModuleInfo64(ctx->process, (DWORD64)address, &ctx->module)) {
			info.module = ctx->module.ModuleName;
		}
	}

	if (flags & (BSTACKTRACE_RESOLVE_FILENAME | BSTACKTRACE_RESOLVE_LINE)) {
		DWORD displacement = 0;
		if (SymGetLineFromAddr64(ctx->process, (DWORD64)address, &displacement, &ctx->line)) {
			if (flags & BSTACKTRACE_RESOLVE_FILENAME) {
				info.filename = ctx->line.FileName;
			}

			if (flags & BSTACKTRACE_RESOLVE_LINE) {
				info.line = (int)ctx->line.LineNumber;
			}
		}
	}

	return info;
}

// }}}
#else

#error "Unsupported platform"

#endif

#endif
