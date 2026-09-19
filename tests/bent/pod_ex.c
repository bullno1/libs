// A user of pod_ex.h: it sees the typed helpers and implements the callback
#include "shared.h"
#include "pod_ex.h"
#include "../../btest.h"

static btest_suite_t pod_ex = {
	.name = "bent/pod_ex",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

static int ex_link_num_serializations = 0;
static void* ex_link_last_ctx = NULL;
static ex_link_t* ex_link_last_comp = NULL;

// Expands to the head the header declared: `comp` is an `ex_link_t*`
BENT_SERIALIZER(ex_link) {
	++ex_link_num_serializations;
	ex_link_last_ctx = ctx;
	ex_link_last_comp = comp;
	return true;
}

// A serializer for a read-only component still gets a mutable pointer: on
// load it is the constructor
BENT_SERIALIZER(ex_cache) {
	(void)ctx;
	_Static_assert(
		_Generic(comp, ex_cache_t*: 1, default: 0),
		"comp must be the component's own type"
	);
	return true;
}

BTEST(pod_ex, save_axis) {
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(ex_pos.def), BENT_COMP_SAVE_NONE);
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(ex_link.def), BENT_COMP_SAVE_CALLBACK);
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(ex_cache.def), BENT_COMP_SAVE_CALLBACK);

	BTEST_EXPECT_EQUAL("%zu", ex_pos.def->size, sizeof(ex_pos_t));
	BTEST_EXPECT_EQUAL("%zu", ex_link.def->size, sizeof(ex_link_t));
}

BTEST(pod_ex, access_axis) {
	bent_world_t* world = fixture.world;
	bent_t ent = bent_create(world);

	_Static_assert(
		_Generic(bent_add_ex_pos(world, ent, NULL), ex_pos_t*: 1, default: 0),
		"RW: bent_add_ex_pos must return a mutable pointer"
	);
	_Static_assert(
		_Generic(bent_get_ex_pos(world, ent), ex_pos_t*: 1, default: 0),
		"RW: bent_get_ex_pos must return a mutable pointer"
	);
	_Static_assert(
		_Generic(bent_add_ex_cache(world, ent, NULL), const ex_cache_t*: 1, default: 0),
		"RO: bent_add_ex_cache must return a const pointer"
	);
	_Static_assert(
		_Generic(bent_get_ex_cache(world, ent), const ex_cache_t*: 1, default: 0),
		"RO: bent_get_ex_cache must return a const pointer"
	);

	ex_pos_t* pos = bent_add_ex_pos(world, ent, &(ex_pos_t){ .x = 1, .y = 2 });
	BTEST_EXPECT_EQUAL("%d", pos->x, 1);
	BTEST_EXPECT_EQUAL("%p", (void*)pos, (void*)bent_get_ex_pos(world, ent));
}

BTEST(pod_ex, serialized_callback_is_typed) {
	bent_world_t* world = fixture.world;
	bent_t ent = bent_create(world);
	ex_link_t* link = bent_add_ex_link(world, ent, &(ex_link_t){ .target = ent });

	int ctx;
	ex_link_num_serializations = 0;
	BTEST_EXPECT(ex_link.def->serialize(&ctx, link));
	BTEST_EXPECT_EQUAL("%d", ex_link_num_serializations, 1);
	BTEST_EXPECT_EQUAL("%p", ex_link_last_ctx, (void*)&ctx);
	BTEST_EXPECT_EQUAL("%p", (void*)ex_link_last_comp, (void*)link);
}
