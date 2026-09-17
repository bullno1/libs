// A reader of ro_pos: it only sees the const helpers
#include "shared.h"
#include "readonly.h"
#include "../../btest.h"

static btest_suite_t readonly = {
	.name = "bent/readonly",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

BTEST(readonly, helpers_return_const) {
	bent_world_t* world = fixture.world;
	bent_t ent = bent_create(world);

	// The typed helpers do not hand out a mutable pointer in this unit
	_Static_assert(
		_Generic(bent_add_ro_pos(world, ent, NULL), const ro_pos_t*: 1, default: 0),
		"bent_add_ro_pos must return a const pointer"
	);
	_Static_assert(
		_Generic(bent_get_ro_pos(world, ent), const ro_pos_t*: 1, default: 0),
		"bent_get_ro_pos must return a const pointer"
	);

	const ro_pos_t* pos = bent_add_ro_pos(world, ent, &(ro_pos_t){ .x = 1, .y = 2 });
	BTEST_EXPECT(pos != NULL);
	BTEST_EXPECT_EQUAL("%d", pos->x, 1);
	BTEST_EXPECT_EQUAL("%d", pos->y, 2);
	BTEST_EXPECT_EQUAL("%p", (const void*)pos, (const void*)bent_get_ro_pos(world, ent));
}

BTEST(readonly, owner_writes_reader_observes) {
	bent_world_t* world = fixture.world;
	bent_t ent = bent_create(world);

	bent_add_ro_pos(world, ent, &(ro_pos_t){ .x = 1, .y = 2 });
	ro_pos_move(world, ent, 3, 4);

	const ro_pos_t* pos = bent_get_ro_pos(world, ent);
	BTEST_EXPECT_EQUAL("%d", pos->x, 4);
	BTEST_EXPECT_EQUAL("%d", pos->y, 6);
}
