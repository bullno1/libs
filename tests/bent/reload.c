#include "shared.h"
#include "../../btest.h"

static struct {
	bent_world_t* world;
	const bent_sys_def_t* def_backup;
} reload_fixture;

static inline void
init_per_reload_test(void) {
	memset(&reload_fixture, 0, sizeof(reload_fixture));

	bent_init(&reload_fixture.world, NULL);
	reload_fixture.def_backup = double_match_system.def;
}

static inline void
cleanup_per_reload_test(void) {
	double_match_system.def = reload_fixture.def_backup;
	bent_init(&reload_fixture.world, NULL);
	bent_cleanup(&reload_fixture.world);
	reload_fixture.world = NULL;
}

static btest_suite_t reload = {
	.name = "bent/reload",
	.init_per_test = init_per_reload_test,
	.cleanup_per_test = cleanup_per_reload_test,
};

BTEST(reload, reload_with_new_match) {
	bent_world_t* world = reload_fixture.world;

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);

	simple_system_t* sys = bent_get_sys_data(world, double_match_system);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 0);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);

	bent_sys_def_t new_def = *reload_fixture.def_backup;
	double_match_system.def = &new_def;

	// Reload
	new_def.require = BENT_COMP_LIST(&basic_component);
	bent_init(&world, NULL);
	sys = bent_get_sys_data(world, double_match_system);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);

	// System data persists between reload
	new_def.require = BENT_COMP_LIST(&basic_component2);
	bent_init(&world, NULL);
	sys = bent_get_sys_data(world, double_match_system);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 1);
}
