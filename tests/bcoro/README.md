# bcoro

Coroutine implemented with switch/case and macro for maximum portability and debuggability.
Reference: https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html

Features:

* Local state variables.
* Support spawning multiple coroutines from the same function.
* No hidden memory allocation.
* Calling into and yielding from a subroutine.
* Premature termination: `coroutin_stop`.
* Cleanup code for each coroutine.
  Guaranteed to be run on both normal and premature termination.

A coroutine can be declared as follow:

```c
// Only a single argument is allowed so a struct is needed if you have multiple
// arguments.
typedef struct {
    int input;
    int* output;
} args;

BCORO(my_coro, args) {
    // The first section must be for variable declaration and all variables
    // must be declared here with the following syntax.
    BCORO_SECTION_VARS
    BCORO_VAR(int, i);

    // The next section is the code.
    BCORO_SECTION_BODY
    printf("Coroutine started\n");

    // Access the argument through BCORO_ARG
    for (i = 0; i < BCORO_ARG.input; ++i) {
        *BCORO_ARG.output = i;
        BCORO_YIELD();  // Return to caller
        printf("Coroutine resumed\n");
    }

    // You can also call another coroutine
    BCORO_YIELD_FROM(another_coro_fn, ((another_args){ 0 }));

    // This section is always run when the coroutine is terminated.
    BCORO_SECTION_CLEANUP
    printf("Coroutine stopped\n");
}
```

Then it can be used like this:

```c
// Allocate space for the coroutine
bcoro_t* coro = malloc(bcoro_mem_size(1024 * 4));  // 4Kib stack
// Spawn the coroutine using the provided storage
int output;
my_coro(coro, (args){ .input = 42, .output = &output });
// If another instance of the coroutine is needed, allocate and use a different bcoro_t.

// Run the coroutine until completion
while (bcoro_resume(coro) != BCORO_TERMINATED) {
    printf("%d\n", output);
}

// Alternatively, a suspended coroutine can be prematurely terminated.
bcoro_stop(coro);
// Its cleanup code will be run in both normal and premature termination.
```

Refer to the example for more info.
