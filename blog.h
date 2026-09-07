#ifndef BLOG_H
#define BLOG_H

/**
 * @file
 * @brief Logging, with short filenames.
 *
 * Messages are written with @ref BLOG_TRACE, @ref BLOG_DEBUG, @ref BLOG_INFO,
 * @ref BLOG_WARN, @ref BLOG_ERROR and @ref BLOG_FATAL.
 * They capture the calling file and line and check the printf-style
 * arguments at compile time.
 *
 * A message is formatted once and handed to every registered logger whose
 * minimum level allows it.
 * Up to @ref BLOG_MAX_NUM_LOGGERS loggers can be added, either custom ones
 * with @ref blog_add_logger or the built-in ones: @ref blog_add_file_logger
 * for any `FILE*` (optionally with ANSI colors) and
 * @ref blog_add_android_logger for logcat.
 *
 * Filenames are shortened relative to the project root.
 * @ref blog_init is given the `__FILE__` of the calling source file along
 * with how many directories deep that file sits in the project and strips
 * the common prefix from every logged path:
 *
 * ```c
 * // In src/main.c, one directory below the project root
 * blog_init(&(blog_options_t){
 *     .current_filename = __FILE__,
 *     .current_depth_in_project = 1,
 * });
 * blog_add_file_logger(BLOG_LEVEL_INFO, &(blog_file_logger_options_t){
 *     .file = stderr,
 *     .with_colors = true,
 * });
 * BLOG_INFO("Hello %s", "world");  // [INFO ][src/main.c:12]: Hello world
 * ```
 *
 * Messages longer than @ref BLOG_LINE_BUF_SIZE are truncated.
 *
 * ## Threading
 *
 * Logging is safe from any thread: each thread formats into its own
 * thread-local buffer.
 * Configuration is not: call @ref blog_init, add loggers and set their levels
 * once, in the main thread, before other threads start logging.
 * A logger is called on whichever thread logs the message, possibly several
 * at once, so a custom logger must bring its own synchronization.
 * The built-in loggers are thread-safe.
 *
 * In **exactly one** source file, define `BLOG_IMPLEMENTATION` before including blog.h.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#ifndef BLOG_API
#define BLOG_API
#endif

/*! Maximum number of loggers, can be overridden */
#ifndef BLOG_MAX_NUM_LOGGERS
#define BLOG_MAX_NUM_LOGGERS 4
#endif

/*! Size of the buffer a message is formatted into, can be overridden */
#ifndef BLOG_LINE_BUF_SIZE
#define BLOG_LINE_BUF_SIZE 1024
#endif

/// @cond INTERNAL
#if defined(__GNUC__) || defined(__clang__)
#	define BLOG_FORMAT_ATTRIBUTE(FMT, VA) __attribute__((format(printf, FMT, VA)))
#	define BLOG_FORMAT_CHECK(...) (void)(sizeof(0))
#else
#	define BLOG_FORMAT_ATTRIBUTE(FMT, VA)
#	define BLOG_FORMAT_CHECK(...) (void)(sizeof(printf(__VA_ARGS__)))
#endif

#if defined(_MSC_VER)
#	define BLOG_THREAD_LOCAL __declspec(thread)
#else
#	define BLOG_THREAD_LOCAL _Thread_local
#endif
/// @endcond

/**
 * Log a printf-style message at the given level from the current location.
 *
 * The level-specific macros below are usually more convenient.
 *
 * @param LEVEL a @ref blog_level_t
 * @param ... format string and arguments
 *
 * @hideinitializer
 */
#define BLOG_WRITE(LEVEL, ...) \
	(BLOG_FORMAT_CHECK(__VA_ARGS__), blog_write(LEVEL, __FILE__, __LINE__, __VA_ARGS__))

/*! Log at @ref BLOG_LEVEL_TRACE @hideinitializer */
#define BLOG_TRACE(...) BLOG_WRITE(BLOG_LEVEL_TRACE, __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_DEBUG @hideinitializer */
#define BLOG_DEBUG(...) BLOG_WRITE(BLOG_LEVEL_DEBUG, __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_INFO @hideinitializer */
#define BLOG_INFO(...)  BLOG_WRITE(BLOG_LEVEL_INFO , __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_WARN @hideinitializer */
#define BLOG_WARN(...)  BLOG_WRITE(BLOG_LEVEL_WARN , __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_ERROR @hideinitializer */
#define BLOG_ERROR(...) BLOG_WRITE(BLOG_LEVEL_ERROR, __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_FATAL @hideinitializer */
#define BLOG_FATAL(...) BLOG_WRITE(BLOG_LEVEL_FATAL, __VA_ARGS__)

/**
 * printf format specifier for a @ref blog_str_t.
 *
 * ```c
 * BLOG_INFO("Message: " BLOG_STR_FMT, BLOG_STR_FMT_ARGS(msg));
 * ```
 *
 * @see BLOG_STR_FMT_ARGS
 */
#define BLOG_STR_FMT "%.*s"
/*! printf arguments for a @ref blog_str_t, to go with @ref BLOG_STR_FMT @hideinitializer */
#define BLOG_STR_FMT_ARGS(X) (X).len, (X).data

/*! Severity of a message, in increasing order */
typedef enum {
    BLOG_LEVEL_TRACE, /*!< Very verbose diagnostics */
    BLOG_LEVEL_DEBUG, /*!< Diagnostics */
    BLOG_LEVEL_INFO,  /*!< Normal operation */
    BLOG_LEVEL_WARN,  /*!< Something unexpected that can be handled */
    BLOG_LEVEL_ERROR, /*!< A failed operation */
    BLOG_LEVEL_FATAL, /*!< The program cannot continue */
} blog_level_t;

/*! A string that is not null-terminated */
typedef struct {
	int len;          /*!< Length in bytes */
	const char* data; /*!< The characters */
} blog_str_t;

/*! Where a message comes from, as seen by a logger */
typedef struct {
	blog_level_t level; /*!< Severity of the message */
	int line;           /*!< Line in the source file */
	blog_str_t file;    /*!< Source file, shortened as configured by @ref blog_init */
} blog_ctx_t;

/**
 * @brief A logger.
 *
 * It is called on the thread that logs the message.
 * When several threads log at once it is called concurrently, so it must
 * synchronize on its own if it needs to.
 * `msg` is only valid for the duration of the call.
 *
 * @param ctx Where the message comes from.
 * @param msg The formatted message.
 * @param userdata The userdata passed to @ref blog_add_logger.
 */
typedef void (*blog_log_fn_t)(
	const blog_ctx_t* ctx,
	blog_str_t msg,
	void* userdata
);

/*! Options for @ref blog_init */
typedef struct {
	/*! `__FILE__` of the source file calling @ref blog_init */
	const char* current_filename;
	/*!
	 * How many directories deep that file is in the project.
	 *
	 * 0 for a file in the project root, 1 for `src/main.c`...
	 * Everything before the project root is stripped from logged filenames.
	 */
	int current_depth_in_project;
} blog_options_t;

/*! Identifier of a logger, negative when it could not be added */
typedef int blog_logger_id_t;

/**
 * @brief Options for @ref blog_add_file_logger.
 *
 * The struct is referenced, not copied, and must stay valid for as long as
 * the logger is in use.
 */
typedef struct {
	FILE* file;       /*!< Where to write, e.g: `stderr` */
	bool with_colors; /*!< Whether to color the level with ANSI escape codes */
} blog_file_logger_options_t;

/**
 * @brief Options for @ref blog_add_android_logger.
 *
 * The struct is referenced, not copied, and must stay valid for as long as
 * the logger is in use.
 */
typedef struct {
	const char* tag; /*!< The logcat tag */
} blog_android_logger_options_t;

/**
 * @brief Initialize the library.
 *
 * Must be called before adding loggers.
 * Not thread-safe: call it once, in the main thread, before logging starts.
 *
 * @param options How to shorten filenames, see @ref blog_options_t.
 */
BLOG_API void
blog_init(const blog_options_t* options);

/**
 * @brief Add a custom logger.
 *
 * Not thread-safe: add loggers once, in the main thread, before other threads
 * start logging.
 *
 * @param min_level Messages below this level are not passed to the logger.
 * @param fn The logger.
 * @param userdata Passed verbatim to the logger.
 *
 * @return The logger's id, or a negative number if @ref BLOG_MAX_NUM_LOGGERS
 *   loggers were already added.
 */
BLOG_API blog_logger_id_t
blog_add_logger(blog_level_t min_level, blog_log_fn_t fn, void* userdata);

/**
 * @brief Add a logger writing one line per message to a `FILE*`.
 *
 * The logger is thread-safe: each message is a single `fprintf` call and the
 * C library locks the stream for the duration of the call, so lines from
 * different threads do not interleave.
 * Adding it is not, see @ref blog_add_logger.
 *
 * @param min_level Messages below this level are not written.
 * @param options Where and how to write, must outlive the logger.
 *
 * @return The logger's id, or a negative number if it could not be added.
 */
BLOG_API blog_logger_id_t
blog_add_file_logger(blog_level_t min_level, const blog_file_logger_options_t* options);

/**
 * @brief Add a logger writing to Android's logcat.
 *
 * The logger is thread-safe: each message is a single `__android_log_print`
 * call.
 * Adding it is not, see @ref blog_add_logger.
 *
 * @param min_level Messages below this level are not written.
 * @param options The logcat tag, must outlive the logger.
 *
 * @return The logger's id, or a negative number if it could not be added.
 *   On other platforms, nothing is added and -1 is returned.
 */
BLOG_API blog_logger_id_t
blog_add_android_logger(blog_level_t min_level, const blog_android_logger_options_t* options);

/**
 * @brief Change the minimum level of a logger.
 *
 * Does nothing for an invalid (negative) id.
 * Not thread-safe with respect to logging: set levels from the main thread
 * before other threads start logging.
 */
BLOG_API void
blog_set_min_log_level(blog_logger_id_t logger, blog_level_t min_level);

/**
 * @brief Log a message, `vprintf` style.
 *
 * Safe to call from any thread.
 *
 * @param level Severity.
 * @param file Source file, typically `__FILE__`.
 *   It is shortened as configured by @ref blog_init.
 * @param line Line in the source file.
 * @param fmt printf format string.
 * @param args Arguments for the format string.
 */
BLOG_API void
blog_vwrite(
	blog_level_t level,
	const char* file,
	int line,
	const char* fmt,
	va_list args
);

/**
 * @brief Log a message, `printf` style.
 *
 * @ref BLOG_WRITE and the level-specific macros fill in `file` and `line`.
 *
 * @see blog_vwrite
 */
BLOG_FORMAT_ATTRIBUTE(4, 5)
static inline void
blog_write(
	blog_level_t level,
	const char* file,
	int line,
	const char* fmt,
	...
) {
	va_list args;
	va_start(args, fmt);
	blog_vwrite(level, file, line, fmt, args);
	va_end(args);
}

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BLOG_IMPLEMENTATION)
#define BLOG_IMPLEMENTATION
#endif

#ifdef BLOG_IMPLEMENTATION

#include <stdarg.h>
#include <string.h>

// Appropriated from https://github.com/rxi/log.c (MIT licensed)
static const char* BLOG_LEVEL_COLOR[] = {
    "[94m", "[36m", "[32m", "[33m", "[31m", "[35m", NULL
};

#define BLOG_TERM_CODE  0x1B
#define BLOG_TERM_RESET "[0m"

static const char* const BLOG_LEVEL_LABEL[] = {
    "TRACE",
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL",
    0
};

typedef struct {
	blog_level_t min_level;
	blog_log_fn_t fn;
	void* userdata;
} blog_logger_t;

static struct {
	blog_logger_t loggers[BLOG_MAX_NUM_LOGGERS];
	int num_loggers;
	blog_options_t options;
	int prefix_len;
} blog_state = { 0 };

// One buffer per thread so that concurrent writes don't garble each other
static BLOG_THREAD_LOCAL char blog_line_buf[BLOG_LINE_BUF_SIZE];

static void
blog_file_write(
	const blog_ctx_t* ctx,
	blog_str_t msg,
	void* userdata
) {
	const blog_file_logger_options_t* options = userdata;

	if (options->with_colors) {
		fprintf(
			options->file,
			"[%c%s%s%c%s][" BLOG_STR_FMT ":%d]: " BLOG_STR_FMT "\n",

			BLOG_TERM_CODE, BLOG_LEVEL_COLOR[ctx->level],
			BLOG_LEVEL_LABEL[ctx->level],
			BLOG_TERM_CODE, BLOG_TERM_RESET,

			BLOG_STR_FMT_ARGS(ctx->file), ctx->line, BLOG_STR_FMT_ARGS(msg)
		);
	} else {
		fprintf(
			options->file,
			"[%s][" BLOG_STR_FMT ":%d]: " BLOG_STR_FMT "\n",
			BLOG_LEVEL_LABEL[ctx->level],
			BLOG_STR_FMT_ARGS(ctx->file), ctx->line,
			BLOG_STR_FMT_ARGS(msg)
		);
	}
}

void
blog_init(const blog_options_t* options) {
	blog_state.options = *options;

	int depth = options->current_depth_in_project + 1;
	const char* current_filename = options->current_filename;
	if (current_filename != NULL) {
		int len = (int)strlen(current_filename);
		int i = len - 1;
		for (; i >= 0; --i) {
			char ch = current_filename[i];
			if (ch == '/' || ch == '\\') {
				--depth;
			}

			if (depth == 0) { break; }
		}

		if (
			i >= 0
			&&
			(current_filename[i] == '/' || current_filename[i] == '\\')
		) {
			i += 1;
		}

		if (depth == 0 && i >= 0) {
			blog_state.prefix_len = i;
		}
	}
}

blog_logger_id_t
blog_add_logger(blog_level_t min_level, blog_log_fn_t fn, void* userdata) {
	if (blog_state.num_loggers < BLOG_MAX_NUM_LOGGERS) {
		int id = blog_state.num_loggers++;
		blog_state.loggers[id] = (blog_logger_t){
			.min_level = min_level,
			.fn = fn,
			.userdata = userdata,
		};
		return id;
	} else {
		return -1;
	}
}

blog_logger_id_t
blog_add_file_logger(blog_level_t min_level, const blog_file_logger_options_t* options) {
	return blog_add_logger(min_level, blog_file_write, (void*)options);
}

void
blog_set_min_log_level(blog_logger_id_t logger, blog_level_t min_level) {
	if (logger >= 0) {
		blog_state.loggers[logger].min_level = min_level;
	}
}

void
blog_vwrite(
	blog_level_t level,
	const char* filename,
	int line,
	const char* fmt,
	va_list args
) {
	filename = filename != NULL ? filename : "<unknown>";
	int filename_len = (int)strlen(filename);
	blog_str_t filename_str = {
		.len = filename_len,
		.data = filename,
	};

	// Shorten filename by common prefix
	if (
		blog_state.prefix_len > 0
		&& filename_len >= blog_state.prefix_len
		&& memcmp(filename, blog_state.options.current_filename, blog_state.prefix_len) == 0
	) {
		filename_str.len -= blog_state.prefix_len;
		filename_str.data += blog_state.prefix_len;
	}

	blog_ctx_t ctx = {
		.file = filename_str,
		.line = line,
		.level = level,
	};

	int msg_len = -1;
	for (int i = 0; i < blog_state.num_loggers; ++i) {
		blog_logger_t* logger = &blog_state.loggers[i];
		if (level >= logger->min_level) {
			// Delay formatting until it's actually needed
			if (msg_len < 0) {
				msg_len = vsnprintf(
					blog_line_buf, sizeof(blog_line_buf),
					fmt, args
				);
				if (msg_len < 0) {
					msg_len = 0;
				} else if (msg_len >= (int)sizeof(blog_line_buf)) {
					msg_len = sizeof(blog_line_buf) - 1;
				}
				blog_line_buf[msg_len] = '\0';
			}

			blog_str_t msg = {
				.len = msg_len,
				.data = blog_line_buf
			};
			logger->fn(&ctx, msg, logger->userdata);
		}
	}
}

#ifdef __ANDROID__
#include <android/log.h>

static void
blog_android_write(
	const blog_ctx_t* ctx,
	blog_str_t msg,
	void* userdata
) {
	int prio;
	switch (ctx->level) {
		case BLOG_LEVEL_TRACE: prio = ANDROID_LOG_VERBOSE; break;
		case BLOG_LEVEL_DEBUG: prio = ANDROID_LOG_DEBUG; break;
		case BLOG_LEVEL_INFO: prio = ANDROID_LOG_INFO; break;
		case BLOG_LEVEL_WARN: prio = ANDROID_LOG_WARN; break;
		case BLOG_LEVEL_ERROR: prio = ANDROID_LOG_ERROR; break;
		case BLOG_LEVEL_FATAL: prio = ANDROID_LOG_FATAL;  break;
		default: prio = ANDROID_LOG_DEFAULT; break;
	}

	const blog_android_logger_options_t* options = userdata;
	__android_log_print(
		prio, options->tag,
		"[" BLOG_STR_FMT ":%d]: " BLOG_STR_FMT,
		BLOG_STR_FMT_ARGS(ctx->file), ctx->line,
		BLOG_STR_FMT_ARGS(msg)
	);
}

#endif

blog_logger_id_t
blog_add_android_logger(blog_level_t min_level, const blog_android_logger_options_t* options) {
#ifdef __ANDROID__
	return blog_add_logger(min_level, blog_android_write, (void*)options);
#else
	(void)min_level;
	(void)options;
	return -1;
#endif
}

#endif
