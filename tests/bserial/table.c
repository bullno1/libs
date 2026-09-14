#include "common.h"
#include "record.h"
#include <string.h>

static btest_suite_t table = {
	.name = "bserial/table",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

// Wire size of a vec2f record body: two [F32][4 bytes]
#define VEC2F_BODY_SIZE (5 * 2)
// [RECORD_DEF][2][SYM_DEF "x"][SYM_DEF "y"]
#define VEC2F_DEF_SIZE (1 + 1 + 3 + 3)
// [RECORD_REF][id]
#define VEC2F_REF_SIZE (1 + 1)

static bserial_status_t
serialize_vec2f_table(bserial_ctx_t* ctx, vec2f_t* rows, int* len) {
	BSERIAL_CHECK_STATUS(bserial_table(ctx, len));
	if (*len > 8) { return BSERIAL_MALFORMED; }
	for (int i = 0; i < *len; ++i) {
		BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rows[i]));
	}

	return bserial_status(ctx);
}

BTEST(table, round_trip) {
	vec2f_t rows[] = {
		{ 1.f, 2.f },
		{ 3.f, 4.f },
		{ 5.f, 6.f },
	};
	int len = sizeof(rows) / sizeof(rows[0]);

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows, &len) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	// [TABLE][len] then the schema once, then only values
	BTEST_ASSERT(common_fixture.mem_out.len == 1 + 1 + VEC2F_DEF_SIZE + VEC2F_BODY_SIZE * (size_t)len);

	ctx = common_fixture_make_in_ctx();
	vec2f_t rows2[8] = { 0 };
	int len2 = 8;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows2, &len2) == BSERIAL_OK);
	BTEST_ASSERT(len2 == len);
	BTEST_ASSERT(memcmp(rows, rows2, sizeof(rows)) == 0);
}

BTEST(table, saves_per_row) {
	vec2f_t rows[] = {
		{ 1.f, 2.f },
		{ 3.f, 4.f },
		{ 5.f, 6.f },
		{ 7.f, 8.f },
	};
	int len = sizeof(rows) / sizeof(rows[0]);

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows, &len) == BSERIAL_OK);
	size_t table_size = common_fixture.mem_out.len;

	// The same rows as an array of records, the schema is already interned
	BTEST_ASSERT(bserial_array(ctx, &len) == BSERIAL_OK);
	for (int i = 0; i < len; ++i) {
		BTEST_ASSERT(serialize_vec2f(ctx, &rows[i]) == BSERIAL_OK);
	}
	size_t array_size = common_fixture.mem_out.len - table_size;

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	// Both have a 2 bytes header. The table pays for one schema definition
	// while the array pays for one reference per element.
	BTEST_ASSERT(table_size == 2 + VEC2F_DEF_SIZE + VEC2F_BODY_SIZE * (size_t)len);
	BTEST_ASSERT(array_size == 2 + (VEC2F_REF_SIZE + VEC2F_BODY_SIZE) * (size_t)len);

	ctx = common_fixture_make_in_ctx();
	vec2f_t rows2[8] = { 0 };
	int len2 = 8;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows2, &len2) == BSERIAL_OK);
	BTEST_ASSERT(len2 == len);
	BTEST_ASSERT(memcmp(rows, rows2, sizeof(rows)) == 0);

	int len3 = 0;
	BTEST_ASSERT(bserial_array(ctx, &len3) == BSERIAL_OK);
	BTEST_ASSERT(len3 == len);
	for (int i = 0; i < len3; ++i) {
		vec2f_t row = { 0 };
		BTEST_ASSERT(serialize_vec2f(ctx, &row) == BSERIAL_OK);
		BTEST_ASSERT(memcmp(&row, &rows[i], sizeof(row)) == 0);
	}
}

BTEST(table, shares_schema) {
	vec2f_t vec = { -1.f, -2.f };
	vec2f_t rows[] = {
		{ 1.f, 2.f },
		{ 3.f, 4.f },
	};
	int len = sizeof(rows) / sizeof(rows[0]);

	// A standalone record defines the schema, the table refers to it
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	size_t vec_size = common_fixture.mem_out.len;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows, &len) == BSERIAL_OK);
	size_t table_size = common_fixture.mem_out.len - vec_size;
	// And a record after the table refers to it as well
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	size_t vec_size2 = common_fixture.mem_out.len - vec_size - table_size;

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	BTEST_ASSERT(vec_size == VEC2F_DEF_SIZE + VEC2F_BODY_SIZE);
	BTEST_ASSERT(table_size == 2 + VEC2F_REF_SIZE + VEC2F_BODY_SIZE * (size_t)len);
	BTEST_ASSERT(vec_size2 == VEC2F_REF_SIZE + VEC2F_BODY_SIZE);

	ctx = common_fixture_make_in_ctx();
	vec2f_t vec2 = { 0 };
	BTEST_ASSERT(serialize_vec2f(ctx, &vec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&vec, &vec2, sizeof(vec)) == 0);

	vec2f_t rows2[8] = { 0 };
	int len2 = 8;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows2, &len2) == BSERIAL_OK);
	BTEST_ASSERT(len2 == len);
	BTEST_ASSERT(memcmp(rows, rows2, sizeof(rows)) == 0);

	vec2f_t vec3 = { 0 };
	BTEST_ASSERT(serialize_vec2f(ctx, &vec3) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&vec, &vec3, sizeof(vec)) == 0);
}

BTEST(table, empty) {
	vec2f_t vec = { -1.f, -2.f };
	int len = 0;

	// An empty table has no schema and does not disturb the ones around it
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_vec2f_table(ctx, NULL, &len) == BSERIAL_OK);
	BTEST_ASSERT(common_fixture.mem_out.len == 2);
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_vec2f_table(ctx, NULL, &len) == BSERIAL_OK);
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	int len2 = 8;
	BTEST_ASSERT(serialize_vec2f_table(ctx, NULL, &len2) == BSERIAL_OK);
	BTEST_ASSERT(len2 == 0);
	vec2f_t vec2 = { 0 };
	BTEST_ASSERT(serialize_vec2f(ctx, &vec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&vec, &vec2, sizeof(vec)) == 0);
	len2 = 8;
	BTEST_ASSERT(serialize_vec2f_table(ctx, NULL, &len2) == BSERIAL_OK);
	BTEST_ASSERT(len2 == 0);
	vec2f_t vec3 = { 0 };
	BTEST_ASSERT(serialize_vec2f(ctx, &vec3) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&vec, &vec3, sizeof(vec)) == 0);
}

BTEST(table, rows_read_by_different_functions) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },
		.vec2f = { 4.f, -3.5f },

		// A table nested in every row
		.table_len = 2,
		.table = {
			{ 1.2f, 1.3f },
			{ 3.4f, -4.5f },
		},
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int len = 3;
	BTEST_ASSERT(bserial_table(ctx, &len) == BSERIAL_OK);
	for (int i = 0; i < len; ++i) {
		BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	}

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	int read_len = 0;
	BTEST_ASSERT(bserial_table(ctx, &read_len) == BSERIAL_OK);
	BTEST_ASSERT(read_len == len);

	original_t rec2 = { 0 };
	BTEST_ASSERT(serialize_original(ctx, &rec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	// Keys are matched per row so the order in code does not matter
	original_t rec_flipped = { 0 };
	BTEST_ASSERT(serialize_original_flipped(ctx, &rec_flipped) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec_flipped, sizeof(rec)) == 0);

	// A row can also skip fields, including the nested table
	original_t rec_with_num = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_num, 2) == BSERIAL_OK);
	BTEST_ASSERT(rec_with_num.num == rec.num);
	BTEST_ASSERT(rec_with_num.table_len == 0);

	// The table has ended, the stream is at the end
	uint64_t extra;
	BTEST_ASSERT(bserial_uint(ctx, &extra) == BSERIAL_IO_ERROR);
}

BTEST(table, skipped) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },
		.vec2f = { 4.f, -3.5f },

		.table_len = 2,
		.table = {
			{ 1.2f, 1.3f },
			{ 3.4f, -4.5f },
		},
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	// Skip over the table then read it in the next record
	original_t rec_with_str = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_str, 0) == BSERIAL_OK);
	BTEST_ASSERT(strcmp(rec_with_str.str, rec.str) == 0);

	original_t rec_with_table = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &rec_with_table, 4) == BSERIAL_OK);
	BTEST_ASSERT(rec_with_table.table_len == rec.table_len);
	BTEST_ASSERT(memcmp(rec_with_table.table, rec.table, sizeof(rec.table[0]) * rec.table_len) == 0);
}

typedef struct {
	int table_len;
	vec2f_t table[8];
} table_holder_t;

static bserial_status_t
serialize_table_holder(bserial_ctx_t* ctx, table_holder_t* holder) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, table) {
			BSERIAL_CHECK_STATUS(serialize_vec2f_table(ctx, holder->table, &holder->table_len));
		}
	}

	return bserial_status(ctx);
}

BTEST(table, skipped_definition_registered) {
	// The only definition of the vec2f schema lives inside a skipped table.
	// Later references must still resolve.
	table_holder_t holder = {
		.table_len = 2,
		.table = {
			{ 1.f, 2.f },
			{ 3.f, 4.f },
		},
	};
	vec2f_t vec = { 5.f, 6.f };

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_table_holder(ctx, &holder) == BSERIAL_OK);
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	// Read the holder with no fields, skipping the whole table
	BSERIAL_RECORD(ctx) {
	}
	BTEST_ASSERT(bserial_status(ctx) == BSERIAL_OK);

	vec2f_t vec2 = { 0 };
	BTEST_ASSERT(serialize_vec2f(ctx, &vec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&vec, &vec2, sizeof(vec)) == 0);
}

typedef struct {
	int value;
	int flag;
} other_row_t;

static bserial_status_t
serialize_other_row(bserial_ctx_t* ctx, other_row_t* row) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, value) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &row->value));
		}

		BSERIAL_KEY(ctx, flag) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &row->flag));
		}
	}

	return bserial_status(ctx);
}

BTEST(table, mismatched_rows) {
	vec2f_t vec = { 1.f, 2.f };
	other_row_t other = { 3, 4 };

	// All rows must have the same keys
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int len = 2;
	BTEST_ASSERT(bserial_table(ctx, &len) == BSERIAL_OK);
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_other_row(ctx, &other) == BSERIAL_MALFORMED);
	BTEST_ASSERT(bserial_status(ctx) == BSERIAL_MALFORMED);
}

BTEST(table, only_records) {
	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int len = 2;
	BTEST_ASSERT(bserial_table(ctx, &len) == BSERIAL_OK);

	uint64_t num = 1;
	BTEST_ASSERT(bserial_uint(ctx, &num) == BSERIAL_MALFORMED);
	BTEST_ASSERT(bserial_status(ctx) == BSERIAL_MALFORMED);
}

BTEST(table, depth_limit) {
	// A table and its row each take one level
	bserial_ctx_config_t config = common_fixture.ctx_config;
	config.max_depth = 2;
	void* mem = barena_malloc(&common_fixture.arena, bserial_ctx_mem_size(config));
	bserial_ctx_t* ctx = bserial_make_ctx(mem, config, NULL, &common_fixture.mem_out.bserial);

	vec2f_t rows[] = {
		{ 1.f, 2.f },
	};
	int len = 1;
	BTEST_ASSERT(serialize_vec2f_table(ctx, rows, &len) == BSERIAL_MALFORMED);
}
