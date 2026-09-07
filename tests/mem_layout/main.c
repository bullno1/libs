#include "../../mem_layout.h"
#include "../../btest.h"
#include <stdlib.h>

static btest_suite_t mem_layout_ = {
	.name = "mem_layout",
};

//! [mem_layout_example]
// A struct with variable-sized members
typedef struct {
	int num_ints;
	int* ints;

	int num_floats;
	float* floats;
} var_struct;

static var_struct*
var_struct_new(int num_ints, int num_floats) {
	// Calculate the size of the entire buffer
	mem_layout_t layout = 0;
	ptrdiff_t base = mem_layout_reserve(&layout, sizeof(var_struct), _Alignof(var_struct));
	ptrdiff_t ints = mem_layout_reserve(&layout, sizeof(int) * num_ints, _Alignof(int));
	ptrdiff_t floats = mem_layout_reserve(&layout, sizeof(float) * num_floats, _Alignof(float));
	size_t mem_required = mem_layout_size(&layout);

	// Now we can allocate and init the struct with a single allocation
	void* buffer = malloc(mem_required);
	var_struct* vs = mem_layout_locate(buffer, base);
	vs->num_ints = num_ints;
	vs->ints = mem_layout_locate(buffer, ints);
	vs->num_floats = num_floats;
	vs->floats = mem_layout_locate(buffer, floats);

	return vs;
}
//! [mem_layout_example]

BTEST(mem_layout_, example) {
	var_struct* vs = var_struct_new(4, 5);
	BTEST_ASSERT(vs != NULL);

	// The base object sits at the start of the buffer and members do not
	// overlap
	BTEST_EXPECT((char*)vs->ints >= (char*)(vs + 1));
	BTEST_EXPECT((char*)vs->floats >= (char*)(vs->ints + vs->num_ints));
	BTEST_EXPECT(((uintptr_t)vs->ints % _Alignof(int)) == 0);
	BTEST_EXPECT(((uintptr_t)vs->floats % _Alignof(float)) == 0);

	for (int i = 0; i < vs->num_ints; ++i) {
		vs->ints[i] = i;
	}
	for (int i = 0; i < vs->num_floats; ++i) {
		vs->floats[i] = (float)i;
	}
	for (int i = 0; i < vs->num_ints; ++i) {
		BTEST_EXPECT_EQUAL("%d", vs->ints[i], i);
	}
	for (int i = 0; i < vs->num_floats; ++i) {
		BTEST_EXPECT_EQUAL("%f", vs->floats[i], (float)i);
	}

	free(vs);
}

BTEST(mem_layout_, alignment) {
	mem_layout_t layout = 0;
	BTEST_EXPECT_EQUAL("%td", mem_layout_reserve(&layout, 1, 1), 0);
	BTEST_EXPECT_EQUAL("%zu", mem_layout_size(&layout), 1);

	// Padding is inserted to satisfy alignment
	BTEST_EXPECT_EQUAL("%td", mem_layout_reserve(&layout, 8, 8), 8);
	BTEST_EXPECT_EQUAL("%zu", mem_layout_size(&layout), 16);

	// No padding when already aligned
	BTEST_EXPECT_EQUAL("%td", mem_layout_reserve(&layout, 4, 4), 16);
	BTEST_EXPECT_EQUAL("%td", mem_layout_reserve(&layout, 3, 1), 20);
	BTEST_EXPECT_EQUAL("%zu", mem_layout_size(&layout), 23);

	BTEST_EXPECT_EQUAL("%td", mem_layout_reserve(&layout, 1, 64), 64);
	BTEST_EXPECT_EQUAL("%zu", mem_layout_size(&layout), 65);
}

BTEST(mem_layout_, locate) {
	char buffer[64];
	BTEST_EXPECT(mem_layout_locate(buffer, 0) == buffer);
	BTEST_EXPECT(mem_layout_locate(buffer, 17) == &buffer[17]);
}
