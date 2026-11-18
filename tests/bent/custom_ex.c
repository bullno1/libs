#include "shared.h"
#include "custom_ex.h"
#include "../../btest.h"

static btest_suite_t custom_ex = {
	.name = "bent/custom_ex",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

BENT_SERIALIZER(cx_gauge) {
	(void)ctx;
	(void)comp;
	return true;
}

BTEST(custom_ex, axes) {
	bent_world_t* world = fixture.world;
	bent_t ent = bent_create(world);

	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(cx_gauge.def), BENT_COMP_SAVE_CALLBACK);
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(cx_counter.def), BENT_COMP_SAVE_NONE);
	BTEST_EXPECT_EQUAL("%zu", cx_gauge.def->size, sizeof(cx_gauge_t));
	BTEST_EXPECT_EQUAL("%zu", cx_counter.def->size, sizeof(cx_counter_t));
	BTEST_EXPECT(cx_gauge.def->init != NULL);
	BTEST_EXPECT(cx_counter.def->init != NULL);

	_Static_assert(
		_Generic(bent_add_cx_gauge(world, ent, NULL), const cx_gauge_t*: 1, default: 0),
		"RO: bent_add_cx_gauge must return a const pointer"
	);
	_Static_assert(
		_Generic(bent_get_cx_gauge(world, ent), const cx_gauge_t*: 1, default: 0),
		"RO: bent_get_cx_gauge must return a const pointer"
	);
	_Static_assert(
		_Generic(bent_add_cx_counter(world, ent, &(cx_counter_arg_t){ 0 }), cx_counter_t*: 1, default: 0),
		"RW: bent_add_cx_counter must take the argument type and return a mutable pointer"
	);
	_Static_assert(
		_Generic(bent_get_cx_counter(world, ent), cx_counter_t*: 1, default: 0),
		"RW: bent_get_cx_counter must return a mutable pointer"
	);
	_Static_assert(
		_Generic((cx_counter_arg_t*)0, cx_counter_arg_t*: 1, default: 0),
		"The argument type must be aliased for BENT_COMP"
	);
}

BTEST(custom_ex, initializer_runs_on_add) {
	bent_world_t* world = fixture.world;

	bent_t full = bent_create(world);
	const cx_gauge_t* gauge = bent_add_cx_gauge(world, full, &(cx_gauge_t){ .max = 10 });
	BTEST_EXPECT_EQUAL("%d", gauge->max, 10);
	BTEST_EXPECT_EQUAL("%d", gauge->current, 10);

	bent_t hurt = bent_create(world);
	gauge = bent_add_cx_gauge(world, hurt, &(cx_gauge_t){ .max = 10, .current = -3 });
	BTEST_EXPECT_EQUAL("%d", gauge->max, 10);
	BTEST_EXPECT_EQUAL("%d", gauge->current, 7);

	bent_t empty = bent_create(world);
	gauge = bent_add_cx_gauge(world, empty, NULL);
	BTEST_EXPECT_EQUAL("%d", gauge->max, 0);
	BTEST_EXPECT_EQUAL("%d", gauge->current, 0);

	bent_t counted = bent_create(world);
	cx_counter_t* counter = bent_add_cx_counter(world, counted, &(cx_counter_arg_t){ .start = 5 });
	BTEST_EXPECT_EQUAL("%d", counter->value, 5);
	BTEST_EXPECT_EQUAL("%d", counter->num_inits, 1);

	bent_t defaulted = bent_create(world);
	counter = bent_add_cx_counter(world, defaulted, NULL);
	BTEST_EXPECT_EQUAL("%d", counter->value, -1);
}

BTEST(custom_ex, initializer_runs_from_a_prefab) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(cx_gauge, { .max = 4, .current = -1 }),
		BENT_COMP(cx_counter, { .start = 9 })
	));

	const cx_gauge_t* gauge = bent_get_cx_gauge(world, ent);
	BTEST_ASSERT(gauge != NULL);
	BTEST_EXPECT_EQUAL("%d", gauge->max, 4);
	BTEST_EXPECT_EQUAL("%d", gauge->current, 3);

	cx_counter_t* counter = bent_get_cx_counter(world, ent);
	BTEST_ASSERT(counter != NULL);
	BTEST_EXPECT_EQUAL("%d", counter->value, 9);
}
