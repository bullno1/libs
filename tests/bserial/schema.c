#include "common.h"
#include "record.h"
#include <string.h>

static btest_suite_t schema = {
	.name = "bserial/schema",
	.init_per_test = common_fixture_init,
	.cleanup_per_test = common_fixture_cleanup,
};

BTEST(schema, interned) {
	vec2f_t a = { 1.f, 2.f };
	vec2f_t b = { 3.f, 4.f };

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_vec2f(ctx, &a) == BSERIAL_OK);
	size_t first = common_fixture.mem_out.len;
	BTEST_ASSERT(serialize_vec2f(ctx, &b) == BSERIAL_OK);
	size_t second = common_fixture.mem_out.len - first;

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	// [RECORD_DEF][2][SYM_DEF "x"][SYM_DEF "y"] then two [F32][4 bytes]
	BTEST_ASSERT(first == 1 + 1 + 3 + 3 + 5 * 2);
	// [RECORD_REF][0] then two [F32][4 bytes]
	BTEST_ASSERT(second == 1 + 1 + 5 * 2);

	ctx = common_fixture_make_in_ctx();
	vec2f_t a2 = { 0 };
	vec2f_t b2 = { 0 };
	BTEST_ASSERT(serialize_vec2f(ctx, &a2) == BSERIAL_OK);
	BTEST_ASSERT(serialize_vec2f(ctx, &b2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&a, &a2, sizeof(a)) == 0);
	BTEST_ASSERT(memcmp(&b, &b2, sizeof(b)) == 0);
}

BTEST(schema, array_of_records) {
	original_t rec = {
		.num = 42069,
		.str = "Hello",
		.array_len = 3,
		.array = { 1, 2, 3 },

		.table_len = 2,
		.table = {
			{ 1.2f, 1.3f },
			{ 3.4f, -4.5f },
		},
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	int len = 2;
	BTEST_ASSERT(bserial_array(ctx, &len) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	int read_len = 0;
	BTEST_ASSERT(bserial_array(ctx, &read_len) == BSERIAL_OK);
	BTEST_ASSERT(read_len == len);

	original_t rec2 = { 0 };
	BTEST_ASSERT(serialize_original(ctx, &rec2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec2, sizeof(rec)) == 0);

	// Elements of the same array can be read by different functions
	original_t rec_flipped = { 0 };
	BTEST_ASSERT(serialize_original_flipped(ctx, &rec_flipped) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&rec, &rec_flipped, sizeof(rec)) == 0);
}

BTEST(schema, skipped_definitions) {
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
	int len = 5;
	BTEST_ASSERT(bserial_array(ctx, &len) == BSERIAL_OK);
	for (int i = 0; i < len; ++i) {
		BTEST_ASSERT(serialize_original(ctx, &rec) == BSERIAL_OK);
	}

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);
	ctx = common_fixture_make_in_ctx();

	// Each element only reads one field. The first element skips the field
	// holding the first vec2f, whose schema definition lives inside the
	// skipped data. Later elements then read vec2f through references, which
	// only works if the skipper registered that definition.
	int read_len = 0;
	BTEST_ASSERT(bserial_array(ctx, &read_len) == BSERIAL_OK);
	BTEST_ASSERT(read_len == len);

	original_t with_str = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &with_str, 0) == BSERIAL_OK);
	BTEST_ASSERT(strcmp(with_str.str, rec.str) == 0);

	original_t with_array = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &with_array, 1) == BSERIAL_OK);
	BTEST_ASSERT(with_array.array_len == rec.array_len);
	BTEST_ASSERT(memcmp(with_array.array, rec.array, sizeof(rec.array[0]) * rec.array_len) == 0);

	original_t with_num = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &with_num, 2) == BSERIAL_OK);
	BTEST_ASSERT(with_num.num == rec.num);

	original_t with_vec2 = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &with_vec2, 3) == BSERIAL_OK);
	BTEST_ASSERT(with_vec2.vec2f.x == rec.vec2f.x);
	BTEST_ASSERT(with_vec2.vec2f.y == rec.vec2f.y);

	original_t with_table = { 0 };
	BTEST_ASSERT(serialize_original_skip(ctx, &with_table, 4) == BSERIAL_OK);
	BTEST_ASSERT(with_table.table_len == rec.table_len);
	BTEST_ASSERT(memcmp(with_table.table, rec.table, sizeof(rec.table[0]) * rec.table_len) == 0);
}

// A leaf has the same keys as a tree so they share a schema. The leaves are
// written as references while the tree's own definition is still open.
typedef struct {
	int value;
	int num_children;
} leaf_t;

typedef struct {
	int value;
	int num_children;
	leaf_t children[4];
} tree_t;

static bserial_status_t
serialize_leaf(bserial_ctx_t* ctx, leaf_t* leaf) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, value) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &leaf->value));
		}

		BSERIAL_KEY(ctx, children) {
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &leaf->num_children));
			if (leaf->num_children != 0) { return BSERIAL_MALFORMED; }
		}
	}

	return bserial_status(ctx);
}

static bserial_status_t
serialize_tree(bserial_ctx_t* ctx, tree_t* tree) {
	BSERIAL_RECORD(ctx) {
		BSERIAL_KEY(ctx, value) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &tree->value));
		}

		BSERIAL_KEY(ctx, children) {
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &tree->num_children));
			if (tree->num_children > 4) { return BSERIAL_MALFORMED; }
			for (int i = 0; i < tree->num_children; ++i) {
				BSERIAL_CHECK_STATUS(serialize_leaf(ctx, &tree->children[i]));
			}
		}
	}

	return bserial_status(ctx);
}

BTEST(schema, nested_shares_schema) {
	tree_t tree = {
		.value = 1,
		.num_children = 3,
		.children = {
			{ .value = 10 },
			{ .value = 20 },
			{ .value = 30 },
		},
	};

	bserial_ctx_t* ctx = common_fixture.out_ctx;
	BTEST_ASSERT(serialize_tree(ctx, &tree) == BSERIAL_OK);
	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);

	// Only one definition in the whole stream:
	// [RECORD_DEF][2][SYM_DEF "value"][SYM_DEF "children"]
	size_t def_size = 1 + 1 + (1 + 1 + 5) + (1 + 1 + 8);
	// [SINT 1][ARRAY 3]
	size_t tree_body_size = 2 + 2;
	// [RECORD_REF][0][SINT n][ARRAY 0]
	size_t leaf_size = 2 + 2 + 2;
	BTEST_ASSERT(common_fixture.mem_out.len == def_size + tree_body_size + leaf_size * 3);

	ctx = common_fixture_make_in_ctx();
	tree_t tree2 = { 0 };
	BTEST_ASSERT(serialize_tree(ctx, &tree2) == BSERIAL_OK);
	BTEST_ASSERT(memcmp(&tree, &tree2, sizeof(tree)) == 0);
}

BTEST(schema, limit) {
	bserial_ctx_config_t config = common_fixture.ctx_config;
	config.max_num_schemas = 1;
	void* mem = barena_malloc(&common_fixture.arena, bserial_ctx_mem_size(config));
	bserial_ctx_t* ctx = bserial_make_ctx(mem, config, NULL, &common_fixture.mem_out.bserial);

	vec2f_t vec = { 1.f, 2.f };
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);

	leaf_t leaf = { 0 };
	BTEST_ASSERT(serialize_leaf(ctx, &leaf) == BSERIAL_MALFORMED);
}

// Key names are compared by address on the write side.
// These buffers give each call site distinct addresses regardless of string
// pooling, and the first key of the two schemas is the same buffer.
static const char key_x[] = "x";
static const char key_y[] = "y";
static const char key_z[] = "z";

static bserial_status_t
serialize_vec2f_other_site(bserial_ctx_t* ctx, vec2f_t* rec) {
	BSERIAL_RECORD(ctx) {
		if (bserial_key(ctx, key_x, sizeof(key_x) - 1)) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->x));
		}

		if (bserial_key(ctx, key_y, sizeof(key_y) - 1)) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->y));
		}
	}

	return bserial_status(ctx);
}

static bserial_status_t
serialize_vec2f_as_xz(bserial_ctx_t* ctx, vec2f_t* rec) {
	BSERIAL_RECORD(ctx) {
		if (bserial_key(ctx, key_x, sizeof(key_x) - 1)) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->x));
		}

		if (bserial_key(ctx, key_z, sizeof(key_z) - 1)) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->y));
		}
	}

	return bserial_status(ctx);
}

BTEST(schema, call_sites) {
	vec2f_t vec = { 1.f, 2.f };
	bserial_ctx_t* ctx = common_fixture.out_ctx;

	// [RECORD_DEF][2][SYM_DEF "x"][SYM_DEF "y"] then two [F32][4 bytes]
	size_t def_size = 1 + 1 + 3 + 3 + 5 * 2;
	// [RECORD_REF][id] then two [F32][4 bytes]
	size_t ref_size = 1 + 1 + 5 * 2;

	// A different call site with the same keys shares the schema
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(common_fixture.mem_out.len == def_size);
	BTEST_ASSERT(serialize_vec2f_other_site(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(common_fixture.mem_out.len == def_size + ref_size);
	BTEST_ASSERT(serialize_vec2f_other_site(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(common_fixture.mem_out.len == def_size + ref_size * 2);
	BTEST_ASSERT(serialize_vec2f(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(common_fixture.mem_out.len == def_size + ref_size * 3);

	// A schema sharing the first key address with another must stay distinct,
	// even when alternating with it.
	// [RECORD_DEF][2][SYM_REF x][SYM_DEF "z"] then two [F32][4 bytes]
	size_t xz_def_size = 1 + 1 + 2 + 3 + 5 * 2;
	BTEST_ASSERT(serialize_vec2f_as_xz(ctx, &vec) == BSERIAL_OK);
	BTEST_ASSERT(common_fixture.mem_out.len == def_size + ref_size * 3 + xz_def_size);
	size_t before = common_fixture.mem_out.len;
	for (int i = 0; i < 3; ++i) {
		BTEST_ASSERT(serialize_vec2f_other_site(ctx, &vec) == BSERIAL_OK);
		BTEST_ASSERT(serialize_vec2f_as_xz(ctx, &vec) == BSERIAL_OK);
	}
	BTEST_ASSERT(common_fixture.mem_out.len == before + ref_size * 6);

	hex_dump(common_fixture.mem_out.mem, common_fixture.mem_out.len);

	ctx = common_fixture_make_in_ctx();
	for (int i = 0; i < 4; ++i) {
		vec2f_t out = { 0 };
		BTEST_ASSERT(serialize_vec2f(ctx, &out) == BSERIAL_OK);
		BTEST_ASSERT(memcmp(&vec, &out, sizeof(vec)) == 0);
	}
	for (int i = 0; i < 4; ++i) {
		vec2f_t xz = { 0 };
		BTEST_ASSERT(serialize_vec2f_as_xz(ctx, &xz) == BSERIAL_OK);
		BTEST_ASSERT(memcmp(&vec, &xz, sizeof(vec)) == 0);
		if (i < 3) {
			vec2f_t xy = { 0 };
			BTEST_ASSERT(serialize_vec2f(ctx, &xy) == BSERIAL_OK);
			BTEST_ASSERT(memcmp(&vec, &xy, sizeof(vec)) == 0);
		}
	}
}
