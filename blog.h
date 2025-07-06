#ifndef BLOG_H
#define BLOG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#ifndef BLOG_API
#define BLOG_API
#endif

#ifndef BLOG_MAX_NUM_LOGGERS
#define BLOG_MAX_NUM_LOGGERS 4
#endif

#ifndef BLOG_LINE_BUF_SIZE
#define BLOG_LINE_BUF_SIZE 1024
#endif

#if defined(__GNUC__) || defined(__clang__)
#	define BLOG_FORMAT_ATTRIBUTE(FMT, VA) __attribute__((format(printf, FMT, VA)))
#	define BLOG_FORMAT_CHECK(...) (void)(sizeof(0))
#else
#	define BLOG_FORMAT_ATTRIBUTE(FMT, VA)
#	define BLOG_FORMAT_CHECK(...) (void)(sizeof(printf(__VA_ARGS__)))
#endif

#define BLOG_WRITE(LEVEL, ...) \
	(BLOG_FORMAT_CHECK(__VA_ARGS__), blog_write(LEVEL, __FILE__, __LINE__, __VA_ARGS__))

#define BLOG_TRACE(...) BLOG_WRITE(BLOG_LEVEL_TRACE, __VA_ARGS__)
#define BLOG_DEBUG(...) BLOG_WRITE(BLOG_LEVEL_DEBUG, __VA_ARGS__)
#define BLOG_INFO(...)  BLOG_WRITE(BLOG_LEVEL_INFO , __VA_ARGS__)
#define BLOG_WARN(...)  BLOG_WRITE(BLOG_LEVEL_WARN , __VA_ARGS__)
#define BLOG_ERROR(...) BLOG_WRITE(BLOG_LEVEL_ERROR, __VA_ARGS__)
#define BLOG_FATAL(...) BLOG_WRITE(BLOG_LEVEL_FATAL, __VA_ARGS__)

#define BLOG_STR_FMT "%.*s"
#define BLOG_STR_FMT_ARGS(X) (X).len, (X).data

typedef enum {
    BLOG_LEVEL_TRACE,
    BLOG_LEVEL_DEBUG,
    BLOG_LEVEL_INFO,
    BLOG_LEVEL_WARN,
    BLOG_LEVEL_ERROR,
    BLOG_LEVEL_FATAL,
} blog_level_t;

typedef struct {
	int len;
	const char* data;
} blog_str_t;

typedef struct {
	blog_level_t level;
	int line;
	blog_str_t file;
} blog_ctx_t;

typedef void (*blog_log_fn_t)(
	const blog_ctx_t* ctx,
	blog_str_t msg,
	void* userdata
);

typedef struct {
	// For shortening path in log
	const char* current_filename;
	int current_depth_in_project;
} blog_options_t;

typedef int blog_logger_id_t;

typedef struct {
	FILE* file;
	bool with_colors;
} blog_file_logger_options_t;

typedef struct {
	const char* tag;
} blog_android_logger_options_t;

BLOG_API void
blog_init(const blog_options_t* options);

BLOG_API blog_logger_id_t
blog_add_logger(blog_level_t min_level, blog_log_fn_t fn, void* userdata);

BLOG_API blog_logger_id_t
blog_add_file_logger(blog_level_t min_level, const blog_file_logger_options_t* options);

BLOG_API blog_logger_id_t
blog_add_android_logger(blog_level_t min_level, const blog_android_logger_options_t* options);

BLOG_API void
blog_set_min_log_level(blog_logger_id_t logger, blog_level_t min_level);

BLOG_API void
blog_vwrite(
	blog_level_t level,
	const char* file,
	int line,
	const char* fmt,
	va_list args
);

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
	char line_buf[BLOG_LINE_BUF_SIZE];
	int line_len;
	int num_loggers;
	blog_options_t options;
	int prefix_len;
} blog_state = { 0 };

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

		if (current_filename[i] == '/' || current_filename[i] == '\\') {
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
					blog_state.line_buf, sizeof(blog_state.line_buf),
					fmt, args
				);
				if (msg_len < 0) {
					msg_len = 0;
				} else if (msg_len >= (int)sizeof(blog_state.line_buf)) {
					msg_len = sizeof(blog_state.line_buf) - 1;
				}
				blog_state.line_buf[msg_len] = '\0';
			}

			blog_str_t msg = {
				.len = msg_len,
				.data = blog_state.line_buf
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
