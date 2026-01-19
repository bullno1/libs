#include "../../barray.h"
#include "../../btest.h"

static btest_suite_t array = {
	.name = "array",
};

BTEST(array, order) {
	barray(char) arr = NULL;
	barray_push(arr, 3, NULL);
	barray_push(arr, 4, NULL);

	BTEST_EXPECT_EQUAL("%d", arr[0], 3);
	BTEST_EXPECT_EQUAL("%d", arr[1], 4);

	barray_free(arr, NULL);
}

BTEST(array, resize_must_zero) {
	barray(char) arr = NULL;
	barray_push(arr, 3, NULL);
	barray_resize(arr, 4, NULL);

	BTEST_EXPECT_EQUAL("%d", arr[0], 3);
	for (int i = 1; i < 4; ++i) {
		BTEST_EXPECT_EQUAL("%d", arr[i], 0);
	}

	barray_free(arr, NULL);
}

BTEST(array, resize_emtpy_array) {
	barray(char) arr = NULL;
	barray_resize(arr, 4, NULL);

	for (int i = 0; i < 4; ++i) {
		BTEST_EXPECT_EQUAL("%d", arr[i], 0);
	}

	barray_free(arr, NULL);
}

BTEST(array, resize_emtpy_array_to_zero) {
	barray(char) arr = NULL;
	barray_resize(arr, 0, NULL);
}

BTEST(array, free_set_array_to_null) {
	barray(char) arr = NULL;
	barray_resize(arr, 4, NULL);

	BTEST_EXPECT(arr != NULL);

	barray_free(arr, NULL);
	BTEST_EXPECT(arr == NULL);
}

#define BLIB_IMPLEMENTATION
#include "../../barray.h"
