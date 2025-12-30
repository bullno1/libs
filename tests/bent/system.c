#include "shared.h"
#include "../../btest.h"

static btest_suite_t system = {
	.name = "bent/system",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

typedef struct {
	int num_adds;
	int num_removes;
} simple_system_t;

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

BENT_DEFINE_SYS(single_match_system1) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component),
};

BENT_DEFINE_SYS(single_match_system2) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component2),
};

BENT_DEFINE_SYS(double_match_system) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component, &basic_component2),
};

BENT_DEFINE_SYS(system_with_exclusion) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&basic_component),
	.exclude = BENT_COMP_LIST(&basic_component2),
	.add = sys_add_entity,
	.remove = sys_remove_entity,
};

BTEST(system, basic_match) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	BTEST_EXPECT(!bent_match(world, single_match_system1, ent));
	BTEST_EXPECT(!bent_match(world, single_match_system2, ent));
	BTEST_EXPECT(!bent_match(world, double_match_system, ent));
	BTEST_EXPECT(!bent_match(world, system_with_exclusion, ent));

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
