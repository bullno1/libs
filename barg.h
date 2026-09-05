#ifndef BARG_H
#define BARG_H

/**
 * @file
 * @brief Command line argument parser.
 *
 * In **exactly one** source file, define `BARG_IMPLEMENTATION` before including barg.h.
 *
 * Declare the options in an array of @ref barg_opt_t, reference it from a
 * @ref barg_t and call @ref barg_parse.
 * Parsed values are written through the parsers assigned to each option
 * (@ref barg_int, @ref barg_boolean, @ref barg_str, @ref barg_array or a
 * custom @ref barg_opt_parser_t).
 *
 * Both `--option value` and `--option=value` are accepted, as well as
 * `-o value` and `-ovalue` for short names.
 * A lone `--` marks the end of options.
 *
 * Use @ref barg_print_result to report an error or print the help text
 * depending on the result of @ref barg_parse.
 *
 * Example:
 *
 * @snippet samples/barg.c barg_opt
 * @snippet samples/barg.c barg_parse
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifndef BARG_API
#define BARG_API
#endif

/**
 * @brief Parser for an option's value.
 *
 * Example:
 *
 * @snippet samples/barg.c barg_custom_parser
 *
 * @see barg_int
 * @see barg_boolean
 * @see barg_str
 * @see barg_array
 */
typedef struct {
	/*! Arbitrary context passed to @ref parse */
	void* userdata;

	/**
	 * @brief Parse the value of an option.
	 *
	 * @param userdata The @ref userdata field.
	 * @param value The raw string value.
	 *   `NULL` when the option is a boolean flag.
	 * @return `NULL` on success, or an error message on failure.
	 */
	const char* (*parse)(void* userdata, const char* value);
} barg_opt_parser_t;

/**
 * @brief A command line option.
 *
 * Example:
 *
 * @snippet samples/barg.c barg_opt
 */
typedef struct {
	/*! Long name, used as `--name` (optional) */
	const char* name;
	/*! Short name, used as `-x` (optional) */
	char short_name;
	/*! One line summary, shown next to the option in the help text (optional) */
	const char* summary;
	/*! Detailed, potentially multiline description, shown in the help text (optional) */
	const char* description;
	/*! Name for the option's value in the help text, defaults to `value` (optional) */
	const char* value_name;
	/*! Whether this option is a boolean flag that takes no value */
	bool boolean;
	/*! Whether this option can be specified more than once */
	bool repeatable;
	/*! Whether to hide this option from the help text */
	bool hidden;
	/*! How to parse this option's value */
	barg_opt_parser_t parser;

	// Private
	/*! @cond */
	int barg__count;
	size_t barg__name_len;
	/*! @endcond */
} barg_opt_t;

/*! Status of a parse */
typedef enum {
	/*! All options were parsed successfully */
	BARG_OK,
	/*! An argument could not be parsed */
	BARG_PARSE_ERROR,
	/*! The help option was invoked and the help text should be shown */
	BARG_SHOW_HELP,
} barg_status_t;

/**
 * @brief Result of a parse.
 *
 * @see barg_parse
 * @see barg_print_result
 */
typedef struct {
	/*! Status of the parse */
	barg_status_t status;
	/**
	 * @brief Index into `argv`.
	 *
	 * On @ref BARG_OK, this is the index of the first positional argument.
	 * It is equal to `argc` when there is none.
	 *
	 * On @ref BARG_PARSE_ERROR, this is the index of the offending argument.
	 */
	int arg_index;
	/*! The offending value on @ref BARG_PARSE_ERROR */
	const char* value;
	/*! The error message on @ref BARG_PARSE_ERROR */
	const char* message;
} barg_result_t;

/**
 * @brief Configuration for @ref barg_array.
 *
 * This must stay alive for the duration of @ref barg_parse.
 */
typedef struct {
	/*! Size in bytes of an array element */
	size_t element_size;
	/**
	 * @brief Parser for a single element.
	 *
	 * Its `userdata` must initially point at the first element of the output
	 * array.
	 * It will be advanced by @ref element_size after each parsed element.
	 */
	barg_opt_parser_t element_parser;
	/*! Capacity of the output array */
	int max_num_elements;

	/**
	 * @brief Number of elements parsed so far.
	 *
	 * Must be initialized to 0.
	 */
	int num_elements;
} barg_array_opts_t;

/*! The argument parser */
typedef struct {
	/*! Number of options in @ref opts */
	int num_opts;
	/*! The options */
	barg_opt_t* opts;
	/*! Whether positional arguments are allowed */
	bool allow_positional;
	/*! Usage line, shown at the top of the help text (optional) */
	const char* usage;
	/*! Summary of the program, shown in the help text (optional) */
	const char* summary;
} barg_t;

/**
 * @brief Parse the command line arguments.
 *
 * `argv[0]` is assumed to be the program name and is skipped.
 * Parsing stops at the first positional argument or at a lone `--`.
 *
 * @param barg The parser.
 * @param argc Number of arguments, as passed to `main`.
 * @param argv The arguments, as passed to `main`.
 * @return The result of the parse.
 *   On @ref BARG_OK, `arg_index` is the index of the first positional argument.
 *
 * Example:
 *
 * @snippet samples/barg.c barg_parse
 *
 * @see barg_print_result
 */
BARG_API barg_result_t
barg_parse(barg_t* barg, int argc, const char* argv[]);

/**
 * @brief Print the result of a parse.
 *
 * On @ref BARG_PARSE_ERROR, print the error message.
 * On @ref BARG_SHOW_HELP, print the help text.
 * Otherwise, print nothing.
 *
 * @param barg The parser previously passed to @ref barg_parse.
 * @param result The result returned by @ref barg_parse.
 * @param file Where to print to.
 */
BARG_API void
barg_print_result(barg_t* barg, barg_result_t result, FILE* file);

/**
 * @brief Create a parser that parses the value as an `int`.
 *
 * @param out Where to store the parsed value.
 *   This must stay alive for the duration of @ref barg_parse.
 * @return The parser.
 */
BARG_API barg_opt_parser_t
barg_int(int* out);

/**
 * @brief Create a parser for a boolean flag.
 *
 * The output is set to `true` when the flag is present.
 * The associated option should have @ref barg_opt_t.boolean set.
 *
 * @param out Where to store the parsed value.
 *   This must stay alive for the duration of @ref barg_parse.
 * @return The parser.
 */
BARG_API barg_opt_parser_t
barg_boolean(bool* out);

/**
 * @brief Create a parser that stores the value as a string.
 *
 * The stored pointer refers to the original string in `argv`, no copy is made.
 *
 * @param out Where to store the parsed value.
 *   This must stay alive for the duration of @ref barg_parse.
 * @return The parser.
 */
BARG_API barg_opt_parser_t
barg_str(const char** out);

/**
 * @brief Create a parser that collects values into a fixed size array.
 *
 * Each occurrence of the option appends one element.
 * The associated option should have @ref barg_opt_t.repeatable set.
 *
 * @param options How to parse the elements.
 *   This must stay alive for the duration of @ref barg_parse.
 * @return The parser.
 *
 * Example:
 *
 * @snippet samples/barg.c barg_array
 *
 * @see barg_array_opts_t
 */
BARG_API barg_opt_parser_t
barg_array(barg_array_opts_t* options);

/**
 * @brief Create the standard help option.
 *
 * This adds `-h, --help` which makes @ref barg_parse return
 * @ref BARG_SHOW_HELP.
 *
 * @return The option, to be included in @ref barg_t.opts.
 *
 * @see barg_print_result
 */
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
	(void)value;
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
