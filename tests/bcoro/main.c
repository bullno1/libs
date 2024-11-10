#define BCORO_IMPLEMENTATION
#include "../../bcoro.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	int input;
	int* output;
} args;

BCORO(bar, args) {
	BCORO_SECTION_VARS
	BCORO_VAR(int, i)

	BCORO_SECTION_BODY
	printf("bar started\n");

	for (i = 0; i <= BCORO_ARG.input; ++i) {
		*BCORO_ARG.output = i;
		BCORO_YIELD();
	}

	BCORO_SECTION_CLEANUP
	printf("bar terminated\n");
}

BCORO(foo, args) {
	BCORO_SECTION_VARS
	BCORO_VAR(int, i)

	BCORO_SECTION_BODY
	printf("foo started\n");

	for (i = 0; i < BCORO_ARG.input; ++i) {
		BCORO_YIELD_FROM(bar, ((args){
			.input = i,
			.output = BCORO_ARG.output,
		}));
	}

	BCORO_SECTION_CLEANUP
	printf("foo terminated\n");
}


int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	bcoro_t* coro = malloc(bcoro_mem_size(1024 * 4));
	int out;
	foo(coro, (args){ .input = 4, .output = &out });

	while (bcoro_resume(coro) != BCORO_TERMINATED) {
		printf("%d\n", out);
	}

	// Premature termination
	printf("----\n");
	bool should_terminate = false;
	foo(coro, (args){.input = 5, .output = &out });
	while (bcoro_resume(coro) != BCORO_TERMINATED) {
		printf("%d\n", out);

		if (should_terminate && out == 3) {
			printf("Terminating coro\n");
			bcoro_stop(coro);
		}

		if (out == 3) {
			should_terminate = true;
		}
	}

	free(coro);
}
