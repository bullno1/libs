#define BCORO_IMPLEMENTATION
#include "../../bcoro.h"
#include <stdio.h>
#include <stdlib.h>

static size_t STACK_SIZE = 1024 * 4;

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

BCORO(early_exit, args) {
	BCORO_SECTION_VARS
	BCORO_VAR(int, i)

	BCORO_SECTION_BODY
	printf("early_exit started\n");

	for (i = 0; i < BCORO_ARG.input; ++i) {
		if (i == 2) {
			printf("Early exit\n");
			BCORO_EXIT();
		}

		BCORO_YIELD_FROM(bar, ((args){
			.input = i,
			.output = BCORO_ARG.output,
		}));
	}

	BCORO_SECTION_CLEANUP
	printf("early_exit terminated\n");
}

typedef struct {
	int input;
	int* output;
	bcoro_t* clone;
} fork_args;

BCORO(coro_fork, fork_args) {
	BCORO_SECTION_VARS
	BCORO_VAR(int, i)
	BCORO_VAR(bool, is_parent)

	BCORO_SECTION_BODY
	printf("coro_fork(%p) started\n", (void*)BCORO_SELF);
	is_parent = true;

	for (i = 0; i < BCORO_ARG.input; ++i) {
		if (i == 2) {
			printf("%p: Forking into %p\n", (void*)BCORO_SELF, (void*)BCORO_ARG.clone);
			BCORO_FORK(BCORO_ARG.clone, STACK_SIZE) {
				printf("%p: Preparing clone\n", (void*)BCORO_SELF);
				// The clone can make deep copy of shared resources here
				is_parent = false;

				// Return control to parent
				BCORO_YIELD();
			}

			printf("%p: Resuming %s\n", (void*)BCORO_SELF, is_parent ? "parent" : "clone");
		}

		if (!is_parent && i == 3) {
			printf("%p: Clone exits early\n", (void*)BCORO_SELF);
			BCORO_EXIT();
		}

		BCORO_YIELD_FROM(bar, ((args){
			.input = i,
			.output = BCORO_ARG.output,
		}));
	}

	BCORO_SECTION_CLEANUP
	printf("coro_fork(%p) terminated\n", (void*)BCORO_SELF);
}


int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	bcoro_t* coro = malloc(bcoro_mem_size(STACK_SIZE));
	int out;
	foo(coro, (args){ .input = 4, .output = &out });

	while (bcoro_resume(coro) != BCORO_TERMINATED) {
		printf("%d\n", out);
	}

	// Premature termination
	printf("----\n");
	bool should_terminate = false;
	foo(coro, (args){.input = 4, .output = &out });
	while (bcoro_resume(coro) != BCORO_TERMINATED) {
		printf("%d\n", out);

		if (should_terminate && out == 2) {
			printf("Terminating coro\n");
			bcoro_stop(coro);
		}

		if (out == 2) {
			should_terminate = true;
		}
	}

	// Self termination
	printf("----\n");
	early_exit(coro, (args){.input = 4, .output = &out });
	while (bcoro_resume(coro) != BCORO_TERMINATED) {
		printf("%d\n", out);
	}

	// Fork
	printf("----\n");
	bcoro_t* coro_clone = malloc(bcoro_mem_size(STACK_SIZE));
	coro_fork(coro, (fork_args){.input = 4, .output = &out, .clone = coro_clone });

	while (bcoro_resume(coro) != BCORO_TERMINATED) {
		printf("%d\n", out);
	}

	printf("----\n");
	while (bcoro_resume(coro_clone) != BCORO_TERMINATED) {
		printf("%d\n", out);
	}

	free(coro_clone);
	free(coro);
}
