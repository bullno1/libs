#ifndef BARG_H
#define BARG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifndef BARG_API
#define BARG_API
#endif

typedef struct {
	void* userdata;
	const char* (*parse)(void* userdata, const char* value);
} barg_opt_parser_t;

typedef struct {
	const char* name;
	char short_name;
	const char* summary;
	const char* description;
	const char* value_name;
	bool boolean;
	bool repeatable;
	bool hidden;
	barg_opt_parser_t parser;

	// Private
	int barg__count;
	size_t barg__name_len;
} barg_opt_t;

typedef enum {
	BARG_OK,
	BARG_PARSE_ERROR,
	BARG_SHOW_HELP,
} barg_status_t;

typedef struct {
	barg_status_t status;
	int arg_index;
	const char* value;
	const char* message;
} barg_result_t;

typedef struct {
	size_t element_size;
	barg_opt_parser_t element_parser;
	int max_num_elements;

	int num_elements;  // Must be initialized to 0
} barg_array_opts_t;

typedef struct {
	int num_opts;
	barg_opt_t* opts;
	bool allow_positional;
	const char* usage;
	const char* summary;
} barg_t;

BARG_API barg_result_t
barg_parse(barg_t* barg, int argc, const char* argv[]);

BARG_API void
barg_print_result(barg_t* barg, barg_result_t result, FILE* file);

BARG_API barg_opt_parser_t
barg_int(int* out);

BARG_API barg_opt_parser_t
barg_boolean(bool* out);

BARG_API barg_opt_parser_t
barg_str(const char** out);

BARG_API barg_opt_parser_t
barg_array(barg_array_opts_t* options);

BARG_API barg_opt_t
barg_opt_help(void);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BARG_IMPLEMENTATION)
#define BARG_IMPLEMENTATION
#endif

#ifdef BARG_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>

#define BARG_MAKE_ERROR(MSG) \
	(barg_result_t){ \
		.status = BARG_PARSE_ERROR, \
		.arg_index = arg_index, \
		.value = argv[arg_index], \
		.message = MSG, \
	}

static char BARG_HELP_MAGIC = 0;

static barg_result_t
barg_try_parse(
	const char* arg,
	barg_opt_t* opt,
	int argc, const char* argv[],
	int arg_index, bool is_long
) {
	if (!opt->repeatable && opt->barg__count >= 1) {
		return BARG_MAKE_ERROR("Option can only be specified once");
	}

	int opt_name_len = is_long ? (int)opt->barg__name_len : 1;
	char separator = arg[opt_name_len];
	if (opt->boolean) {
		if (separator == '\0') {
			if (opt->parser.userdata == &BARG_HELP_MAGIC) {
				return (barg_result_t) {
					.status = BARG_SHOW_HELP,
					.arg_index = arg_index,
				};
			}

			const char* error = opt->parser.parse(opt->parser.userdata, NULL);

			if (error == NULL) {
				++opt->barg__count;
				return (barg_result_t){
					.status = BARG_OK,
					.arg_index = arg_index,
				};
			} else {
				return BARG_MAKE_ERROR(error);
			}
		} else {
			return BARG_MAKE_ERROR("Invalid usage of a boolean flag");
		}
	} else {
		const char* value;
		if (separator == '=') {  // --opt=value
			value = arg + opt_name_len + 1;
		} else if (separator == '\0') {  // --opt value
			if ((arg_index + 1) < argc) {  // Grab the next arg
				value = argv[++arg_index];
			} else {
				return BARG_MAKE_ERROR("Option must be followed by a value");
			}
		} else {  //--optsomethingelse
			if (is_long) {
				return BARG_MAKE_ERROR("Unknown option");
			} else {  // -ovalue
				value = &arg[1];
			}
		}

		const char* error = opt->parser.parse(opt->parser.userdata, value);
		if (error == NULL) {
			++opt->barg__count;
			return (barg_result_t){
				.status = BARG_OK,
				.arg_index = arg_index,
			};
		} else {
			return (barg_result_t){
				.status = BARG_PARSE_ERROR,
				.arg_index = arg_index,
				.value = value,
				.message = error,
			};
		}
	}
}

static barg_result_t
barg_handle_positional(barg_t* barg, int arg_index, int argc, const char** argv) {
	if (arg_index >= argc || barg->allow_positional) {
		return (barg_result_t){
			.status = BARG_OK,
			.arg_index = arg_index,
		};
	} else {
		return BARG_MAKE_ERROR("Positional arguments are not allowed");
	}
}

barg_result_t
barg_parse(barg_t* barg, int argc, const char* argv[]) {
	int num_opts = barg->num_opts;
	barg_opt_t* opts = barg->opts;
	for (int i = 0; i < num_opts; ++i) {
		opts[i].barg__count = 0;
		if (opts[i].name != NULL) {
			opts[i].barg__name_len = strlen(opts[i].name);
		} else {
			opts[i].barg__name_len = 0;
		}
	}

	int arg_index = 1;
	for (; arg_index < argc; ++arg_index) {
		const char* arg = argv[arg_index];
		if (arg[0] == '-') {
			if (arg[1] == '-') {
				if (arg[2] == '\0') {  // End of options
					return barg_handle_positional(barg, ++arg_index, argc, argv);
				} else {  // Long name
					arg += 2;  // Skip '--'
					barg_opt_t* opt = NULL;
					// Find a matching opt
					for (int opt_index = 0; opt_index < num_opts; ++opt_index) {
						const char* name = opts[opt_index].name;
						size_t name_len = opts[opt_index].barg__name_len;
						size_t arg_len = strlen(arg);

						if (arg_len >= name_len && memcmp(name, arg, name_len) == 0) {
							opt = &opts[opt_index];
							break;
						}
					}

					if (opt == NULL) {
						return BARG_MAKE_ERROR("Unknown option");
					}

					barg_result_t result = barg_try_parse(
						arg, opt,
						argc, argv,
						arg_index, true
					);

					if (result.status == BARG_OK) {
						arg_index = result.arg_index;
					} else {
						return result;
					}
				}
			} else {  // Short name
				arg += 1;  // Skip '-'
				barg_opt_t* opt = NULL;
				// Find a matching opt
				for (int opt_index = 0; opt_index < num_opts; ++opt_index) {
					char short_name = opts[opt_index].short_name;
					if (short_name != '\0' && short_name == arg[0]) {
						opt = &opts[opt_index];
						break;
					}
				}

				if (opt == NULL) {
					return BARG_MAKE_ERROR("Unknown option");
				}

				barg_result_t result = barg_try_parse(
					arg, opt,
					argc, argv,
					arg_index, false
				);

				if (result.status == BARG_OK) {
					arg_index = result.arg_index;
				} else {
					return result;
				}
			}
		} else {  // Positional
			return barg_handle_positional(barg, arg_index, argc, argv);
		}
	}

	return barg_handle_positional(barg, arg_index, argc, argv);
}

static const char*
barg_parse_int(void* userdata, const char* value) {
	char* end;
	errno = 0;
	long result = strtol(value, &end, 0);
	if (*end != '\0') {
		return "Invalid number";
	}

	if (errno != 0 || result < INT_MIN || result > INT_MAX) {
		return "Value out of range";
	}

	*(int*)userdata = (int)result;

	return NULL;
}

barg_opt_parser_t
barg_int(int* out) {
	return (barg_opt_parser_t){
		.userdata = out,
		.parse = barg_parse_int,
	};
}

static const char*
barg_parse_boolean(void* userdata, const char* value) {
	*(bool*)(userdata) = true;
	return NULL;
}

barg_opt_parser_t
barg_boolean(bool* out) {
	return (barg_opt_parser_t){
		.userdata = out,
		.parse = barg_parse_boolean,
	};
}

static const char*
barg_parse_str(void* userdata, const char* value) {
	*(const char**)(userdata) = value;
	return NULL;
}

barg_opt_parser_t
barg_str(const char** out) {
	return (barg_opt_parser_t){
		.userdata = out,
		.parse = barg_parse_str,
	};
}

static const char*
barg_parse_array(void* userdata, const char* value) {
	barg_array_opts_t* options = userdata;
	if (options->num_elements >= options->max_num_elements) {
		return "Array has too many elements";
	}

	const char* error = options->element_parser.parse(
		options->element_parser.userdata,
		value
	);
	if (error != NULL) { return error; }

	++options->num_elements;
	options->element_parser.userdata = (void*)(
		(uintptr_t)options->element_parser.userdata
		+ (uintptr_t)(options->element_size)
	);
	return NULL;
}

barg_opt_parser_t
barg_array(barg_array_opts_t* options) {
	return (barg_opt_parser_t){
		.userdata = options,
		.parse = barg_parse_array,
	};
}

barg_opt_t
barg_opt_help(void) {
	return (barg_opt_t){
		.name = "help",
		.short_name = 'h',
		.summary = "Display this message and exit",
		.boolean = true,
		.parser = { .userdata = &BARG_HELP_MAGIC },
	};
}


static void
barg_print_help(barg_t* barg, FILE* file) {
	if (barg->usage != NULL) {
		fprintf(file, "Usage: %s\n", barg->usage);
	}

	if (barg->summary != NULL) {
		if (barg->usage != NULL) {
			fprintf(file, "\n");
		}
		fprintf(file, "%s\n", barg->summary);
	}

	bool printed_header = false;
	for (int i = 0; i < barg->num_opts; ++i) {
		const barg_opt_t* opt = &barg->opts[i];
		if (opt->hidden) { continue; }

		if (!printed_header) {
			if (barg->summary != NULL) {
				fprintf(file, "\n");
			}
			fprintf(file, "Options:\n");
			printed_header = true;
		}

		fprintf(file, "\n");

		if (opt->short_name != 0) {
			fprintf(file, "-%c", opt->short_name);
		}

		if (opt->name != NULL) {
			if (opt->short_name != 0) {
				fprintf(file, ", ");
			}
			fprintf(file, "--%s", opt->name);
		}

		if (!opt->boolean) {
			fprintf(
				file, "=<%s>",
				opt->value_name != NULL ? opt->value_name : "value"
			);
		}

		if (opt->summary != NULL) {
			fprintf(file, ": %s", opt->summary);
		}

		fprintf(file, "\n");

		if (opt->description != NULL) {
			fprintf(file, "\n");
			const char* line_start = opt->description;
			while (*line_start != '\0') {
				const char* line_end = line_start;
				while (*line_end != '\n' && *line_end != '\0') {
					++line_end;
				}

				fprintf(file, "  %.*s\n", (int)(line_end - line_start), line_start);
				line_start = *line_end == '\0' ? line_end : line_end + 1;
			}
		}
	}
}

void
barg_print_result(barg_t* barg, barg_result_t result, FILE* file) {
	if (result.status == BARG_PARSE_ERROR) {
		fprintf(
			file,
			"Error at argument #%d: %s (%s)\n",
			result.arg_index, result.message, result.value
		);
	} else if (result.status == BARG_SHOW_HELP) {
		barg_print_help(barg, file);
	}
}

#endif
