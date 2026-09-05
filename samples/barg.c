#include "../barg.h"
#include <stdio.h>
#include <string.h>

// Note: Ignore the `//! [label]` markers, they are for for Doxygen
// They are pushed out of the way so you don't need to pay attention to them

// A custom parser can be used for values not covered by the built-in ones
//!                                                                             [barg_custom_parser]
typedef enum {
	COLOR_AUTO,
	COLOR_ALWAYS,
	COLOR_NEVER,
} color_t;

static const char*
parse_color(void* userdata, const char* value) {
	color_t* color = userdata;

	if (strcmp(value, "auto") == 0) {
		*color = COLOR_AUTO;
	} else if (strcmp(value, "always") == 0) {
		*color = COLOR_ALWAYS;
	} else if (strcmp(value, "never") == 0) {
		*color = COLOR_NEVER;
	} else {
		// The error message will be shown to the user
		return "Invalid color mode";
	}

	// NULL means success
	return NULL;
}
//!                                                                             [barg_custom_parser]

int
main(int argc, const char* argv[]) {
	// Storage for the parsed values, initialized with defaults
	int level = 0;
	bool verbose = false;
	const char* output = "a.out";
	color_t color = COLOR_AUTO;

	// A repeatable option can collect its values into an array
//!                                                                             [barg_array]
	const char* defines[4];
	barg_array_opts_t defines_opts = {
		.element_size = sizeof(defines[0]),
		// Point the element parser at the first element
		.element_parser = barg_str(&defines[0]),
		.max_num_elements = sizeof(defines) / sizeof(defines[0]),
	};
//!                                                                             [barg_array]

	// Declare the options
//!                                                                             [barg_opt]
	barg_opt_t opts[] = {
		{
			.name = "optimize",
			.short_name = 'O',
			.value_name = "level",
			.summary = "Optimization level",
			.parser = barg_int(&level),
		},
		{
			.name = "verbose",
			.short_name = 'v',
			.summary = "Print more details",
			.boolean = true,
			.parser = barg_boolean(&verbose),
		},
		{
			.name = "output",
			.short_name = 'o',
			.value_name = "file",
			.summary = "Where to write the output",
			.parser = barg_str(&output),
		},
		{
			.name = "define",
			.short_name = 'D',
			.value_name = "name",
			.summary = "Define a macro",
			.repeatable = true,
			.parser = barg_array(&defines_opts),
		},
		{
			.name = "color",
			.value_name = "mode",
			.summary = "When to use colors",
			.description =
				"One of: auto, always, never.\n"
				"Defaults to auto.",
			.parser = {
				.userdata = &color,
				.parse = parse_color,
			},
		},
		// The standard --help option
		barg_opt_help(),
	};
//!                                                                             [barg_opt]

//!                                                                             [barg_parse]
	barg_t barg = {
		.opts = opts,
		.num_opts = sizeof(opts) / sizeof(opts[0]),
		.allow_positional = true,
		.usage = "barg [options] [--] [files]",
		.summary = "Sample program for barg",
	};

	barg_result_t result = barg_parse(&barg, argc, argv);
	if (result.status != BARG_OK) {
		// Print the help text or the error message accordingly
		barg_print_result(&barg, result, stderr);
		return result.status == BARG_SHOW_HELP ? 0 : 1;
	}

	// On success, arg_index is the index of the first positional argument
	for (int i = result.arg_index; i < argc; ++i) {
		printf("input: %s\n", argv[i]);
	}
//!                                                                             [barg_parse]

	printf("optimize: %d\n", level);
	printf("verbose: %s\n", verbose ? "true" : "false");
	printf("output: %s\n", output);
	printf("color: %d\n", (int)color);
	for (int i = 0; i < defines_opts.num_elements; ++i) {
		printf("define: %s\n", defines[i]);
	}

	return 0;
}

#define BLIB_IMPLEMENTATION
#include "../barg.h"
