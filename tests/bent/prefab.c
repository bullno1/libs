// The component registrations live in this unit
#define BENT_DEFINE_COMPONENTS
#include "shared.h"
#include "../../btest.h"

typedef struct { int x, y; } pos_t;
typedef struct { int hp; } hp_t;

BENT_TRANSIENT_POD_COMP(pos, pos_t)
BENT_DEFINE_COMP_ADDER(pos, pos_t)
BENT_DEFINE_COMP_GETTER(pos, pos_t)

BENT_TRANSIENT_POD_COMP(hp, hp_t)
BENT_DEFINE_COMP_ADDER(hp, hp_t)
BENT_DEFINE_COMP_GETTER(hp, hp_t)

BENT_TAG_COMP(hostile)
BENT_TAG_COMP(spawner)

// A prefab kept at file scope: the list and its arguments have static storage
static bent_prefab_t goblin = BENT_PREFAB(
	BENT_COMP(pos, { .x = 3, .y = 4 }),
	BENT_COMP(hp, { .hp = 7 }),
	BENT_COMP(hostile)
);

typedef struct {
	int num_adds;
	int num_removes;
	// What the add callback observed on the last entity
	bool saw_hp;
	bool saw_hostile;
	int hp_seen;
} observer_t;

static void
observer_add(void* userdata, bent_world_t* world, bent_t entity) {
	observer_t* sys = userdata;
	++sys->num_adds;
	sys->saw_hp = bent_has(world, entity, hp);
	sys->saw_hostile = bent_has(world, entity, hostile);
	hp_t* hp_data = bent_get_hp(world, entity);
	sys->hp_seen = hp_data != NULL ? hp_data->hp : -1;
}

static void
observer_remove(void* userdata, bent_world_t* world, bent_t entity) {
	observer_t* sys = userdata;
	++sys->num_removes;
}

// Matches on pos alone: the interesting one, it can see whether the rest of
// the list is already there
BENT_DEFINE_SYS(pos_observer) = {
	.size = sizeof(observer_t),
	.require = BENT_COMP_LIST(&pos),
	.add = observer_add,
	.remove = observer_remove,
};

BENT_DEFINE_SYS(pos_hp_observer) = {
	.size = sizeof(observer_t),
	.require = BENT_COMP_LIST(&pos, &hp),
	.add = observer_add,
	.remove = observer_remove,
};

// Only excludes, so it matches an empty entity and gets every new one
BENT_DEFINE_SYS(peaceful_observer) = {
	.size = sizeof(observer_t),
	.exclude = BENT_COMP_LIST(&hostile),
	.add = observer_add,
	.remove = observer_remove,
};

typedef struct {
	bent_t child;
} spawner_t;

// Spawns from inside a callback: the notifications must be queued
static void
spawner_add(void* userdata, bent_world_t* world, bent_t entity) {
	spawner_t* sys = userdata;
	sys->child = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(pos, { .x = 1, .y = 1 }),
		BENT_COMP(hp, { .hp = 2 })
	));
}

BENT_DEFINE_SYS(spawner_sys) = {
	.size = sizeof(spawner_t),
	.require = BENT_COMP_LIST(&spawner),
	.add = spawner_add,
};

static btest_suite_t prefab = {
	.name = "bent/prefab",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

BTEST(prefab, adds_every_component) {
	bent_world_t* world = fixture.world;

	pos_t initial_pos = { .x = 5, .y = 6 };
	bent_t ent = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(pos, initial_pos),  // A value
		BENT_COMP(hp, { .hp = 30 }),  // Or an initializer
		BENT_COMP(hostile)  // Tag
	));

	BTEST_EXPECT(bent_is_active(world, ent));
	BTEST_EXPECT(bent_has(world, ent, pos));
	BTEST_EXPECT(bent_has(world, ent, hp));
	BTEST_EXPECT(bent_has(world, ent, hostile));
	BTEST_EXPECT(!bent_has(world, ent, spawner));
	BTEST_EXPECT_EQUAL("%d", 5, bent_get_pos(world, ent)->x);
	BTEST_EXPECT_EQUAL("%d", 6, bent_get_pos(world, ent)->y);
	BTEST_EXPECT_EQUAL("%d", 30, bent_get_hp(world, ent)->hp);

	// The query lists are current
	BTEST_EXPECT_EQUAL("%d", 1, count_query(world, bent_query(world, BENT_COMP_LIST(&pos, &hp, &hostile), NULL)));
}

BTEST(prefab, no_arg_zeroes) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create_from(world, BENT_PREFAB(BENT_COMP(pos), BENT_COMP(hp)));
	BTEST_EXPECT_EQUAL("%d", 0, bent_get_pos(world, ent)->x);
	BTEST_EXPECT_EQUAL("%d", 0, bent_get_pos(world, ent)->y);
	BTEST_EXPECT_EQUAL("%d", 0, bent_get_hp(world, ent)->hp);
}

BTEST(prefab, systems_see_the_complete_entity) {
	bent_world_t* world = fixture.world;
	observer_t* by_pos = bent_get_sys_data(world, pos_observer);
	observer_t* by_pos_hp = bent_get_sys_data(world, pos_hp_observer);

	// For contrast: one add at a time, the pos system is told before hp exists
	bent_t piecemeal = bent_create(world);
	bent_add_pos(world, piecemeal, &(pos_t){ 0 });
	bent_add_hp(world, piecemeal, &(hp_t){ .hp = 1 });
	BTEST_EXPECT_EQUAL("%d", 1, by_pos->num_adds);
	BTEST_EXPECT(!by_pos->saw_hp);
	BTEST_EXPECT_EQUAL("%d", 1, by_pos_hp->num_adds);

	bent_t spawned = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(pos, { .x = 1 }),
		BENT_COMP(hp, { .hp = 42 }),
		BENT_COMP(hostile)
	));
	(void)spawned;
	BTEST_EXPECT_EQUAL("%d", 2, by_pos->num_adds);
	BTEST_EXPECT(by_pos->saw_hp);
	BTEST_EXPECT(by_pos->saw_hostile);
	BTEST_EXPECT_EQUAL("%d", 42, by_pos->hp_seen);
	BTEST_EXPECT_EQUAL("%d", 2, by_pos_hp->num_adds);
	BTEST_EXPECT_EQUAL("%d", 0, by_pos->num_removes);
	BTEST_EXPECT_EQUAL("%d", 0, by_pos_hp->num_removes);
}

BTEST(prefab, excluded_from_the_start) {
	bent_world_t* world = fixture.world;
	observer_t* peaceful = bent_get_sys_data(world, peaceful_observer);

	// For contrast: it gets the empty entity, then loses it
	bent_t piecemeal = bent_create(world);
	BTEST_EXPECT_EQUAL("%d", 1, peaceful->num_adds);
	bent_add(world, piecemeal, hostile, NULL);
	BTEST_EXPECT_EQUAL("%d", 1, peaceful->num_removes);

	// Created with what it excludes: it never hears about the entity
	bent_create_from(world, goblin);
	BTEST_EXPECT_EQUAL("%d", 1, peaceful->num_adds);
	BTEST_EXPECT_EQUAL("%d", 1, peaceful->num_removes);
	BTEST_EXPECT_EQUAL("%d", 0, count_query(world, bent_sys_query(world, peaceful_observer)));

	// Created without: told once, with everything in place
	bent_create_from(world, BENT_PREFAB(BENT_COMP(hp, { .hp = 5 })));
	BTEST_EXPECT_EQUAL("%d", 2, peaceful->num_adds);
	BTEST_EXPECT_EQUAL("%d", 5, peaceful->hp_seen);
	BTEST_EXPECT_EQUAL("%d", 1, peaceful->num_removes);
	BTEST_EXPECT_EQUAL("%d", 1, count_query(world, bent_sys_query(world, peaceful_observer)));
}

BTEST(prefab, inside_a_callback) {
	bent_world_t* world = fixture.world;
	observer_t* by_pos = bent_get_sys_data(world, pos_observer);
	observer_t* by_pos_hp = bent_get_sys_data(world, pos_hp_observer);
	spawner_t* spawner_data = bent_get_sys_data(world, spawner_sys);

	bent_t parent = bent_create_from(world, BENT_PREFAB(BENT_COMP(spawner)));
	bent_t child = spawner_data->child;

	BTEST_EXPECT(!bent_is_invalid(child));
	BTEST_EXPECT(bent_is_active(world, child));
	BTEST_EXPECT(child.index != parent.index);
	BTEST_EXPECT_EQUAL("%d", 2, bent_get_hp(world, child)->hp);

	// Delivered once the outer callback returned, still as a whole
	BTEST_EXPECT_EQUAL("%d", 1, by_pos->num_adds);
	BTEST_EXPECT(by_pos->saw_hp);
	BTEST_EXPECT_EQUAL("%d", 2, by_pos->hp_seen);
	BTEST_EXPECT_EQUAL("%d", 1, by_pos_hp->num_adds);
	BTEST_EXPECT_EQUAL("%d", 1, count_query(world, bent_query(world, BENT_COMP_LIST(&pos), NULL)));
}

BTEST(prefab, prefab) {
	bent_world_t* world = fixture.world;

	bent_t a = bent_create_from(world, goblin);
	bent_t b = bent_create_from(world, goblin);

	BTEST_EXPECT(bent_has(world, a, hostile));
	BTEST_EXPECT(bent_has(world, b, hostile));
	BTEST_EXPECT_EQUAL("%d", 3, bent_get_pos(world, a)->x);
	BTEST_EXPECT_EQUAL("%d", 7, bent_get_hp(world, b)->hp);

	// Separate storage, the prefab is only copied from
	bent_get_hp(world, a)->hp = 1;
	BTEST_EXPECT_EQUAL("%d", 7, bent_get_hp(world, b)->hp);
	bent_t c = bent_create_from(world, goblin);
	BTEST_EXPECT_EQUAL("%d", 7, bent_get_hp(world, c)->hp);
}

BTEST(prefab, add_from) {
	bent_world_t* world = fixture.world;
	observer_t* by_pos = bent_get_sys_data(world, pos_observer);
	observer_t* by_pos_hp = bent_get_sys_data(world, pos_hp_observer);

	bent_t ent = bent_create(world);
	bent_add_pos(world, ent, &(pos_t){ .x = 1, .y = 2 });
	BTEST_EXPECT_EQUAL("%d", 1, by_pos->num_adds);

	bent_add_from(world, ent, BENT_PREFAB(
		BENT_COMP(pos, { .x = 9, .y = 9 }),  // Already there, left alone
		BENT_COMP(hp, { .hp = 5 }),
		BENT_COMP(hostile)
	));

	BTEST_EXPECT_EQUAL("%d", 1, bent_get_pos(world, ent)->x);
	BTEST_EXPECT_EQUAL("%d", 5, bent_get_hp(world, ent)->hp);
	BTEST_EXPECT(bent_has(world, ent, hostile));
	// Not told again about pos, told once about the rest
	BTEST_EXPECT_EQUAL("%d", 1, by_pos->num_adds);
	BTEST_EXPECT_EQUAL("%d", 1, by_pos_hp->num_adds);
	BTEST_EXPECT(by_pos_hp->saw_hostile);

	// An empty list, or one with nothing new, is a noop
	bent_add_from(world, ent, BENT_PREFAB(BENT_COMP(hp)));
	bent_add_from(world, ent, (bent_prefab_entry_t[]){ { 0 } });
	BTEST_EXPECT_EQUAL("%d", 5, bent_get_hp(world, ent)->hp);
	BTEST_EXPECT_EQUAL("%d", 1, by_pos_hp->num_adds);

	bent_destroy(world, ent);
	BTEST_EXPECT_EQUAL("%d", 1, by_pos->num_removes);
	BTEST_EXPECT_EQUAL("%d", 1, by_pos_hp->num_removes);
}
