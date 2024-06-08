#include "../../mem_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

typedef struct {
    int num_ints;
    int* ints;

    int num_floats;
    float* floats;
} var_struct;

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	// Suppose we want to init var_struct with these params;
	int num_ints = 4;
	int num_floats = 5;

	// Calculate the size of the entire buffer
	mem_layout_t layout = { 0 };
	ptrdiff_t base = mem_layout_reserve(&layout, sizeof(var_struct), _Alignof(var_struct));
	ptrdiff_t ints = mem_layout_reserve(&layout, sizeof(int) * num_ints, _Alignof(int));
	ptrdiff_t floats = mem_layout_reserve(&layout, sizeof(float) * num_floats, _Alignof(float));
	size_t mem_required = mem_layout_size(&layout);

	// Now we can allocate and init the struct
	void* buffer = malloc(mem_required);
	var_struct* vs = mem_layout_locate(buffer, base);
	vs->num_ints = num_ints;
	vs->ints = mem_layout_locate(buffer, ints);
	vs->num_floats = num_floats;
	vs->floats = mem_layout_locate(buffer, floats);

	for (int i = 0; i < num_ints; ++i) {
		vs->ints[i] = i;
	}
	for (int i = 0; i < num_floats; ++i) {
		vs->floats[i] = i;
	}

	printf("Size = %zu\n", mem_required);
	printf("ints =");
	for (int i = 0; i < num_ints; ++i) {
		printf(" %d", vs->ints[i]);
	}
	printf("\n");

	printf("floats =");
	for (int i = 0; i < num_floats; ++i) {
		printf(" %f", vs->floats[i]);
	}
	printf("\n");

	free(buffer);

	return 0;
}
