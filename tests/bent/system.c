#include "shared.h"
#include "../../btest.h"

static btest_suite_t system = {
	.name = "bent/system",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

enum {
	UPDATE_PHASE_A = 1 << 0,
	UPDATE_PHASE_B = 1 << 1,
};

static void
sys_add_entity(void* data, bent_world_t* world, bent_t entity) {
	simple_system_t* sys = data;
	++sys->num_adds;
}

static void
sys_remove_entity(void* data, bent_world_t* world, bent_t entity) {
	simple_system_t* sys = data;
	++sys->num_removes;
}

static void
sys_update(
	void* userdata,
	bent_world_t* world,
	bent_mask_t update_mask,
	bent_t* entities,
	bent_index_t num_entities
) {
	simple_system_t* sys = userdata;
	++sys->num_updates;
}

BENT_DEFINE_SYS(single_match_system1) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component),
	.update = sys_update,
	.update_mask = UPDATE_PHASE_A,
};

BENT_DEFINE_SYS(single_match_system2) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component2),
	.update = sys_update,
	.update_mask = UPDATE_PHASE_B,
};

BENT_DEFINE_SYS(double_match_system) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component, &basic_component2),
	.update = sys_update,
	.update_mask = UPDATE_PHASE_A | UPDATE_PHASE_B,
	.add = sys_add_entity,
	.remove = sys_remove_entity,
};

BENT_DEFINE_SYS(system_with_exclusion) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component),
	.exclude = BENT_COMP_LIST(&basic_component2),
	.add = sys_add_entity,
	.remove = sys_remove_entity,
	.update = sys_update,
};

// Empty system to test edge case behavior
BENT_DEFINE_SYS(dummy) = {
	.size = 0,
};

BTEST(system, basic_match) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	BTEST_EXPECT(!bent_match(world, single_match_system1, ent));
	BTEST_EXPECT(!bent_match(world, single_match_system2, ent));
	BTEST_EXPECT(!bent_match(world, double_match_system, ent));
	BTEST_EXPECT(!bent_match(world, system_with_exclusion, ent));
	BTEST_EXPECT(!bent_match(world, dummy, ent));

	bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT(bent_match(world, single_match_system1, ent));
	BTEST_EXPECT(!bent_match(world, single_match_system2, ent));
	BTEST_EXPECT(!bent_match(world, double_match_system, ent));
	BTEST_EXPECT(bent_match(world, system_with_exclusion, ent));

	bent_add(world, ent, basic_component2, NULL);
	BTEST_EXPECT(bent_match(world, single_match_system1, ent));
	BTEST_EXPECT(bent_match(world, single_match_system2, ent));
	BTEST_EXPECT(bent_match(world, double_match_system, ent));
	BTEST_EXPECT(!bent_match(world, system_with_exclusion, ent));

	bent_remove(world, ent, basic_component);
	BTEST_EXPECT(!bent_match(world, single_match_system1, ent));
	BTEST_EXPECT(bent_match(world, single_match_system2, ent));
	BTEST_EXPECT(!bent_match(world, double_match_system, ent));
	BTEST_EXPECT(!bent_match(world, system_with_exclusion, ent));

	bent_destroy(world, ent);
	BTEST_EXPECT(!bent_match(world, single_match_system1, ent));
	BTEST_EXPECT(!bent_match(world, single_match_system2, ent));
	BTEST_EXPECT(!bent_match(world, double_match_system, ent));
	BTEST_EXPECT(!bent_match(world, system_with_exclusion, ent));
}

BTEST(system, add_remove_callback) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	simple_system_t* sys = bent_get_sys_data(world, system_with_exclusion);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 0);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);

	bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);

	bent_add(world, ent, basic_component2, NULL);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 1);

	bent_remove(world, ent, basic_component2);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 1);

	bent_destroy(world, ent);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 2);
}

BTEST(system, update_mask) {
	bent_world_t* world = fixture.world;

	simple_system_t* a = bent_get_sys_data(world, single_match_system1);
	simple_system_t* b = bent_get_sys_data(world, single_match_system2);
	simple_system_t* c = bent_get_sys_data(world, double_match_system);

	BTEST_EXPECT_EQUAL("%d", a->num_updates, 0);
	BTEST_EXPECT_EQUAL("%d", b->num_updates, 0);
	BTEST_EXPECT_EQUAL("%d", c->num_updates, 0);

	bent_run(world, UPDATE_PHASE_A);
	BTEST_EXPECT_EQUAL("%d", a->num_updates, 1);
	BTEST_EXPECT_EQUAL("%d", b->num_updates, 0);
	BTEST_EXPECT_EQUAL("%d", c->num_updates, 1);

	bent_run(world, UPDATE_PHASE_B);
	BTEST_EXPECT_EQUAL("%d", a->num_updates, 1);
	BTEST_EXPECT_EQUAL("%d", b->num_updates, 1);
	BTEST_EXPECT_EQUAL("%d", c->num_updates, 2);
}

BTEST(system, dont_care) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);
	BTEST_EXPECT(!bent_match(world, dummy, ent));

	bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT(!bent_match(world, dummy, ent));
}

BENT_DECLARE_COMP(comp_to_be_removed)
BENT_DEFINE_POD_COMP(comp_to_be_removed, int)

static void
entity_still_has_component_on_sys_remove_callback(void* data, bent_world_t* world, bent_t entity) {
	BTEST_EXPECT(bent_has(world, entity, comp_to_be_removed));
	BTEST_EXPECT(bent_get(world, entity, comp_to_be_removed) != NULL);
}

BENT_DEFINE_SYS(sys_entity_still_has_component_on_sys_remove_callback) = {
	.require = BENT_COMP_LIST(&comp_to_be_removed),
	.remove = entity_still_has_component_on_sys_remove_callback,
};

BTEST(system, entity_still_has_component_on_sys_remove_callback) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);
	bent_add(world, ent, comp_to_be_removed, NULL);
	bent_remove(world, ent, comp_to_be_removed);
}
