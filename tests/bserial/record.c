#include "common.h"
#include <assert.h>
#include <string.h>

static suite_t record = {
	.name = "record",
	.init = common_fixture_init,
	.cleanup = common_fixture_cleanup,
};

typedef struct {
	int num;
	char str[256];

	int array_len;
	int array[8];
} original_t;

static bserial_status_t
serialize_original(bserial_ctx_t* ctx, original_t* record) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, num) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &record->num));
		}

		BSERIAL_KEY(ctx, str) {
			uint64_t len = strlen(record->str);
			BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));
			if (len >= sizeof(record->str)) { return BSERIAL_MALFORMED; }
			BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, record->str));
			record->str[len] = '\0';
		}

		BSERIAL_KEY(ctx, array) {
			uint64_t len = (uint64_t)record->array_len;
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
			if (len >= (sizeof(record->array) / sizeof(record->array[0]))) {
				return BSERIAL_MALFORMED;
			}
			record->array_len = (int)len;
			for (int i = 0; i < record->array_len; ++i) {
				BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &record->array[i]));
			}
		}
	}

	return bserial_status(ctx);
}

static bserial_status_t
serialize_original_flipped(bserial_ctx_t* ctx, original_t* record) {
	// Order of keys does not matter
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, str) {
			uint64_t len = strlen(record->str);
			BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));
			if (len >= sizeof(record->str)) { return BSERIAL_MALFORMED; }
			BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, record->str));
			record->str[len] = '\0';
		}

		BSERIAL_KEY(ctx, array) {
			uint64_t len = (uint64_t)record->array_len;
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
			if (len >= (sizeof(record->array) / sizeof(record->array[0]))) {
				return BSERIAL_MALFORMED;
			}
			record->array_len = (int)len;
			for (int i = 0; i < record->array_len; ++i) {
				BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &record->array[i]));
			}
		}

		BSERIAL_KEY(ctx, num) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &record->num));
		}
	}

	return bserial_status(ctx);
}

TEST(record, round_trip) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 }
	};
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	original_t rec2 = { 0 };
	assert(serialize_original(ctx, &rec2) == BSERIAL_OK);
	assert(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	// Order of keys should not matter
	original_t rec_flipped = { 0 };
	assert(serialize_original_flipped(ctx, &rec_flipped) == BSERIAL_OK);
	assert(memcmp(&rec, &rec_flipped, sizeof(rec)) == 0);
	assert(rec_flipped.array_len == 3);
	assert(rec_flipped.array[0] == 1);
	assert(rec_flipped.array[1] == 2);
	assert(rec_flipped.array[2] == 3);
}

static bserial_status_t
serialize_original_skip(bserial_ctx_t* ctx, original_t* record, int selector) {
	// Depending on the selector, only 1 of the 3 fields will be deserialized.
	BSERIAL_RECORD(ctx) {
		if (selector == 0) {
			BSERIAL_KEY(ctx, str) {
				uint64_t len = strlen(record->str);
				BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));
				if (len >= sizeof(record->str)) { return BSERIAL_MALFORMED; }
				BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, record->str));
				record->str[len] = '\0';
			}
		}

		if (selector == 1) {
			BSERIAL_KEY(ctx, array) {
				uint64_t len = (uint64_t)record->array_len;
				BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
				if (len >= (sizeof(record->array) / sizeof(record->array[0]))) {
					return BSERIAL_MALFORMED;
				}
				record->array_len = (int)len;
				for (int i = 0; i < record->array_len; ++i) {
					BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &record->array[i]));
				}
			}
		}

		if (selector == 2) {
			BSERIAL_KEY(ctx, num) {
				BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &record->num));
			}
		}
	}

	return bserial_status(ctx);
}

TEST(record, missing_fields) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 }
	};
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);
	assert(serialize_original(ctx, &rec) == BSERIAL_OK);

	ctx = common_fixture_make_in_ctx();

	original_t rec_with_str = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_str, 0) == BSERIAL_OK);
	original_t rec_with_array = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_array, 1) == BSERIAL_OK);
	original_t rec_with_num = { 0 };
	assert(serialize_original_skip(ctx, &rec_with_num, 2) == BSERIAL_OK);
}