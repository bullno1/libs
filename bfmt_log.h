#ifndef BFMT_LOG_H
#define BFMT_LOG_H

/**
 * @file
 * @brief Logging with the formatting of @ref bfmt.h through @ref blog.h.
 *
 * @code{.c}
 * BFMT_INFO("Copied ", num_files, " files to ", path);
 * // [INFO ][src/main.c:12]: Copied 3 files to /tmp
 * @endcode
 *
 * Everything @ref bfmt_print accepts works here: @ref bfmt for options,
 * @ref bfmt_with for a specific formatter and the types listed in
 * @ref BFMT_USER_TYPES.
 *
 * Messages longer than @ref BLOG_LINE_BUF_SIZE are truncated.
 *
 * In **exactly one** source file, define `BFMT_LOG_IMPLEMENTATION` before
 * including bfmt_log.h.
 * `BLIB_IMPLEMENTATION` does that for this header, bfmt.h and blog.h at once.
 */

#include "bfmt.h"
#include "blog.h"

/*! Linkage of the API functions, e.g: `__declspec(dllexport)` */
#ifndef BFMT_LOG_API
#define BFMT_LOG_API
#endif

/**
 * Log the values as one message at the given level from the current location.
 *
 * The level-specific macros are usually more convenient.
 *
 * @param LEVEL a @ref blog_level_t
 * @param ... the values to print, as for @ref bfmt_print
 *
 * @hideinitializer
 */
#define BFMT_LOG(LEVEL, ...) \
	bfmt_log( \
		LEVEL, __FILE__, __LINE__, \
		(bfmt__element_t[]){ bfmt__map(bfmt__to_element, __VA_ARGS__) { 0 } } \
	)

/*! Log at @ref BLOG_LEVEL_TRACE @hideinitializer */
#define BFMT_TRACE(...) BFMT_LOG(BLOG_LEVEL_TRACE, __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_DEBUG @hideinitializer */
#define BFMT_DEBUG(...) BFMT_LOG(BLOG_LEVEL_DEBUG, __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_INFO @hideinitializer */
#define BFMT_INFO(...)  BFMT_LOG(BLOG_LEVEL_INFO , __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_WARN @hideinitializer */
#define BFMT_WARN(...)  BFMT_LOG(BLOG_LEVEL_WARN , __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_ERROR @hideinitializer */
#define BFMT_ERROR(...) BFMT_LOG(BLOG_LEVEL_ERROR, __VA_ARGS__)
/*! Log at @ref BLOG_LEVEL_FATAL @hideinitializer */
#define BFMT_FATAL(...) BFMT_LOG(BLOG_LEVEL_FATAL, __VA_ARGS__)

/**
 * @brief Log a list of values as one message.
 *
 * Nothing is formatted unless @ref blog_is_enabled for the level.
 *
 * @param level Severity.
 * @param file Source file, typically `__FILE__`.
 * @param line Line in the source file.
 * @param elements The values, terminated by an element without a formatter,
 *   as @ref bfmt_print builds them.
 */
BFMT_LOG_API void
bfmt_log(
	blog_level_t level,
	const char* file,
	int line,
	const bfmt__element_t* elements
);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BFMT_LOG_IMPLEMENTATION)
#define BFMT_LOG_IMPLEMENTATION
#endif

#if defined(BFMT_LOG_IMPLEMENTATION) && !defined(BFMT_LOG_IMPLEMENTED)
#define BFMT_LOG_IMPLEMENTED

#include <string.h>

typedef struct {
	int len;
	char buf[BLOG_LINE_BUF_SIZE];
} bfmt_log_line_t;

static void
bfmt_log_write(void* userdata, const char* str, int len) {
	bfmt_log_line_t* line = userdata;

	// Truncate at the same length as blog's own messages
	int space = (int)sizeof(line->buf) - line->len;
	if (len > space) { len = space; }

	memcpy(line->buf + line->len, str, (size_t)len);
	line->len += len;
}

void
bfmt_log(
	blog_level_t level,
	const char* file,
	int line,
	const bfmt__element_t* elements
) {
	if (!blog_is_enabled(level)) { return; }

	bfmt_log_line_t msg;
	msg.len = 0;
	bfmt_stream_t stream = { .write = bfmt_log_write, .userdata = &msg };
	bfmt__print_stream(&stream, elements);

	blog_write_raw(level, file, line, msg.buf, msg.len);
}

#endif
