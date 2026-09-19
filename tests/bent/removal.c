// A removed component stays, data included, until every system has been told
// The registrations live in this unit
#define BENT_DEFINE_COMPONENTS
#include "shared.h"
#include "../../btest.h"

// Written into the data by cleanup so that a read after it is caught
#define POISON -1000

typedef struct { int value; } tracked_t;

static int num_tracked_cleanups;

static void
tracked_cleanup(void* data) {
	++num_tracked_cleanups;
	((tracked_t*)data)->value = POISON;
}

BENT_DEFINE_COMP(tracked) = {
	.size = sizeof(tracked_t),
	.cleanup = tracked_cleanup,
	.flags = BENT_COMP_TRANSIENT,
};

// Only the systems below know about these
BENT_TAG_COMP(partner)
BENT_TAG_COMP(orphan)

BENT_MSG(strip_msg) { bool partner_too; bool add_back; };

typedef struct {
	int num_adds;
	int num_removes;
	// What the last remove callback saw
	bool saw_tracked;
	bool saw_partner;
	int value_seen;
	// What the handler saw right after removing
	bool still_has_after_remove;
	void* added_back;
} removal_sys_t;

static void
removal_sys_add(void* userdata, bent_world_t* world, bent_t entity) {
	removal_sys_t* sys = userdata;
	++sys->num_adds;
}

static void
removal_sys_remove(void* userdata, bent_world_t* world, bent_t entity) {
	removal_sys_t* sys = userdata;
	++sys->num_removes;
	sys->saw_tracked = bent_has(world, entity, tracked);
	sys->saw_partner = bent_has(world, entity, partner);
	tracked_t* data = bent_get(world, entity, tracked);
	sys->value_seen = data != NULL ? data->value : 0;
}

static void
tracked_pair_on_strip(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	removal_sys_t* sys = userdata;
	const strip_msg_t* strip = msg;

	bent_remove(world, entity, tracked);
	// Again: nothing more happens
	bent_remove(world, entity, tracked);
	sys->still_has_after_remove = bent_has(world, entity, tracked)
		&& bent_get(world, entity, tracked) != NULL;

	if (strip->partner_too) {
		bent_remove(world, entity, partner);
	}
	if (strip->add_back) {
		sys->added_back = bent_add(world, entity, tracked, &(tracked_t){ .value = 99 });
	}
}

// Needs both: its remove callback should still see both
BENT_DEFINE_SYS(tracked_pair_sys) = {
	.size = sizeof(removal_sys_t),
	.require = BENT_COMP_LIST(&tracked, &partner),
	.add = removal_sys_add,
	.remove = removal_sys_remove,
	.handlers = BENT_MSG_HANDLERS(
		{ &strip_msg, tracked_pair_on_strip }
	),
};

// Marks the entity from its remove callback
static void
orphan_maker_remove(void* userdata, bent_world_t* world, bent_t entity) {
	removal_sys_remove(userdata, world, entity);
	bent_add(world, entity, orphan, NULL);
}

BENT_DEFINE_SYS(orphan_maker_sys) = {
	.size = sizeof(removal_sys_t),
	.require = BENT_COMP_LIST(&tracked),
	.remove = orphan_maker_remove,
};

// Can never match: tracked is gone by the time orphan appears
BENT_DEFINE_SYS(tracked_orphan_sys) = {
	.size = sizeof(removal_sys_t),
	.require = BENT_COMP_LIST(&tracked, &orphan),
	.add = removal_sys_add,
	.remove = removal_sys_remove,
};

static void
init_per_removal_test(void) {
	num_tracked_cleanups = 0;
	bent_test_tolerate_asserts = false;
	bent_test_num_failed_asserts = 0;
	init_per_test();
}

static void
cleanup_per_removal_test(void) {
	cleanup_per_test();
	bent_test_tolerate_asserts = false;
}

static btest_suite_t removal = {
	.name = "bent/removal",
	.init_per_test = init_per_removal_test,
	.cleanup_per_test = cleanup_per_removal_test,
};

static bent_t
make_pair(bent_world_t* world) {
	bent_t ent = bent_create(world);
	bent_add(world, ent, tracked, &(tracked_t){ .value = 7 });
	bent_add(world, ent, partner, NULL);
	return ent;
}

BTEST(removal, from_ordinary_code) {
	bent_world_t* world = fixture.world;
	removal_sys_t* pair = bent_get_sys_data(world, tracked_pair_sys);
	bent_t ent = make_pair(world);

	bent_remove(world, ent, tracked);

	BTEST_EXPECT_EQUAL("%d", pair->num_removes, 1);
	BTEST_EXPECT(pair->saw_tracked);
	BTEST_EXPECT_EQUAL("%d", pair->value_seen, 7);
	// All done by the time it returns
	BTEST_EXPECT(!bent_has(world, ent, tracked));
	BTEST_EXPECT(bent_get(world, ent, tracked) == NULL);
	BTEST_EXPECT_EQUAL("%d", num_tracked_cleanups, 1);
}

BTEST(removal, from_a_callback) {
	bent_world_t* world = fixture.world;
	removal_sys_t* pair = bent_get_sys_data(world, tracked_pair_sys);
	bent_t ent = make_pair(world);

	bent_send(world, ent, strip_msg, { 0 });

	// Until told, it was still there for the handler that removed it
	BTEST_EXPECT(pair->still_has_after_remove);
	// And for the remove callback, data included
	BTEST_EXPECT_EQUAL("%d", pair->num_removes, 1);
	BTEST_EXPECT(pair->saw_tracked);
	BTEST_EXPECT_EQUAL("%d", pair->value_seen, 7);

	BTEST_EXPECT(!bent_has(world, ent, tracked));
	BTEST_EXPECT(bent_get(world, ent, tracked) == NULL);
	BTEST_EXPECT(bent_has(world, ent, partner));
	BTEST_EXPECT_EQUAL("%d", num_tracked_cleanups, 1);
	BTEST_EXPECT_EQUAL("%d", count_query(world, bent_sys_query(world, tracked_pair_sys)), 0);
}

BTEST(removal, several_from_a_callback) {
	bent_world_t* world = fixture.world;
	removal_sys_t* pair = bent_get_sys_data(world, tracked_pair_sys);
	bent_t ent = make_pair(world);

	bent_send(world, ent, strip_msg, { .partner_too = true });

	// Told once, about the first removal, with the second still in place
	BTEST_EXPECT_EQUAL("%d", pair->num_removes, 1);
	BTEST_EXPECT(pair->saw_tracked);
	BTEST_EXPECT(pair->saw_partner);
	BTEST_EXPECT_EQUAL("%d", pair->value_seen, 7);

	BTEST_EXPECT(!bent_has(world, ent, tracked));
	BTEST_EXPECT(!bent_has(world, ent, partner));
	BTEST_EXPECT_EQUAL("%d", num_tracked_cleanups, 1);
}

BTEST(removal, add_from_remove_callback) {
	bent_world_t* world = fixture.world;
	removal_sys_t* maker = bent_get_sys_data(world, orphan_maker_sys);
	removal_sys_t* never = bent_get_sys_data(world, tracked_orphan_sys);

	bent_t ent = bent_create(world);
	bent_add(world, ent, tracked, &(tracked_t){ .value = 3 });
	bent_remove(world, ent, tracked);

	BTEST_EXPECT_EQUAL("%d", maker->num_removes, 1);
	BTEST_EXPECT_EQUAL("%d", maker->value_seen, 3);
	BTEST_EXPECT(bent_has(world, ent, orphan));
	BTEST_EXPECT(!bent_has(world, ent, tracked));

	// The addition is not mixed up with the component on its way out
	BTEST_EXPECT_EQUAL("%d", never->num_adds, 0);
	BTEST_EXPECT_EQUAL("%d", never->num_removes, 0);
	BTEST_EXPECT_EQUAL("%d", count_query(world, bent_sys_query(world, tracked_orphan_sys)), 0);
}

BTEST(removal, add_back_before_told) {
	bent_world_t* world = fixture.world;
	removal_sys_t* pair = bent_get_sys_data(world, tracked_pair_sys);
	bent_t ent = make_pair(world);
	void* data = bent_get(world, ent, tracked);

	// Not supported: it asserts, and without assertion nothing happens
	bent_test_tolerate_asserts = true;
	bent_send(world, ent, strip_msg, { .add_back = true });
	bent_test_tolerate_asserts = false;

	BTEST_EXPECT_EQUAL("%d", bent_test_num_failed_asserts, 1);
	// It got the component that was still there, untouched
	BTEST_EXPECT(pair->added_back == data);
	BTEST_EXPECT_EQUAL("%d", pair->value_seen, 7);
	// And the removal went through
	BTEST_EXPECT_EQUAL("%d", pair->num_removes, 1);
	BTEST_EXPECT_EQUAL("%d", pair->num_adds, 1);
	BTEST_EXPECT(!bent_has(world, ent, tracked));
	BTEST_EXPECT_EQUAL("%d", num_tracked_cleanups, 1);
}

BTEST(removal, then_destroy_from_a_callback) {
	bent_world_t* world = fixture.world;
	removal_sys_t* pair = bent_get_sys_data(world, tracked_pair_sys);
	bent_t ent = make_pair(world);
	bent_t other = make_pair(world);

	bent_send(world, ent, strip_msg, { 0 });
	bent_destroy(world, ent);
	bent_destroy(world, other);

	// Once each, whichever way it goes
	BTEST_EXPECT_EQUAL("%d", pair->num_removes, 2);
	BTEST_EXPECT_EQUAL("%d", pair->value_seen, 7);
	BTEST_EXPECT_EQUAL("%d", num_tracked_cleanups, 2);
}
