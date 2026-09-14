#include "shared.h"
#include "../../btest.h"
#include <stdlib.h>

static btest_suite_t query = {
	.name = "bent/query",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

// Defined in system.c
BENT_DECLARE_SYS(system_with_exclusion)
BENT_DECLARE_SYS(dummy)

// Whether an iteration yields the entity
static bool
query_yields(bent_world_t* world, bent_query_t query, bent_t entity) {
	bool found = false;
	BENT_FOREACH_QUERY(e, world, query) {
		found = found || bent_equal(e, entity);
	}
	return found;
}

static int
count(bent_world_t* world, bent_query_t query) {
	return count_query(world, query);
}

BTEST(query, interned) {
	bent_world_t* world = fixture.world;

	bent_query_t q1 = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	bent_query_t q2 = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	BTEST_EXPECT(q1.id != 0);
	BTEST_EXPECT_EQUAL("%d", q1.id, q2.id);

	// Order of the list does not matter
	bent_query_t q3 = bent_query(world, BENT_COMP_LIST(&basic_component, &basic_component2), NULL);
	bent_query_t q4 = bent_query(world, BENT_COMP_LIST(&basic_component2, &basic_component), NULL);
	BTEST_EXPECT_EQUAL("%d", q3.id, q4.id);
	BTEST_EXPECT(q3.id != q1.id);

	// Require and exclude are distinct
	bent_query_t q5 = bent_query(world, NULL, BENT_COMP_LIST(&basic_component));
	BTEST_EXPECT(q5.id != q1.id);

	// Same thing from masks
	bent_query_t q6 = bent_query_masks(
		world,
		bent_bitset_from_comp_list(BENT_COMP_LIST(&basic_component)),
		(bent_bitset_t){ 0 }
	);
	BTEST_EXPECT_EQUAL("%d", q6.id, q1.id);

	// The empty query
	bent_query_t none = { 0 };
	BTEST_EXPECT_EQUAL("%d", count(world, none), 0);
	bent_t ent = bent_create(world);
	BTEST_EXPECT(!bent_query_match(world, none, ent));
}

BTEST(query, tracks_changes) {
	bent_world_t* world = fixture.world;

	bent_query_t q = bent_query(
		world,
		BENT_COMP_LIST(&basic_component),
		BENT_COMP_LIST(&basic_component2)
	);

	bent_t ent1 = bent_create(world);
	bent_t ent2 = bent_create(world);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);
	BTEST_EXPECT(!bent_query_match(world, q, ent1));

	bent_add(world, ent1, basic_component, NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT(bent_query_match(world, q, ent1));

	bent_add(world, ent2, basic_component, NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 2);

	// Excluded
	bent_add(world, ent2, basic_component2, NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT(query_yields(world, q, ent1));
	BTEST_EXPECT(!bent_query_match(world, q, ent2));

	bent_remove(world, ent2, basic_component2);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 2);

	bent_destroy(world, ent1);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT(query_yields(world, q, ent2));
	BTEST_EXPECT(!bent_query_match(world, q, ent1));

	// Reusing the slot must not resurrect the membership
	bent_t ent3 = bent_create(world);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT(!bent_query_match(world, q, ent3));

	bent_remove(world, ent2, basic_component);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);
}

BTEST(query, created_after_entities) {
	bent_world_t* world = fixture.world;

	bent_t ent1 = bent_create(world);
	bent_add(world, ent1, basic_component, NULL);
	bent_t ent2 = bent_create(world);
	bent_t ent3 = bent_create(world);
	bent_add(world, ent3, basic_component, NULL);
	bent_add(world, ent3, basic_component2, NULL);

	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 2);
	BTEST_EXPECT(query_yields(world, q, ent1));
	BTEST_EXPECT(!query_yields(world, q, ent2));
	BTEST_EXPECT(query_yields(world, q, ent3));

	// And it keeps tracking from there
	bent_add(world, ent2, basic_component, NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 3);
}

BTEST(query, match_all) {
	bent_world_t* world = fixture.world;

	bent_query_t all = bent_query(world, NULL, NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, all), 0);

	bent_t ent1 = bent_create(world);
	bent_t ent2 = bent_create(world);
	bent_create(world);
	BTEST_EXPECT_EQUAL("%d", count(world, all), 3);

	bent_add(world, ent1, basic_component, NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, all), 3);

	bent_destroy(world, ent2);
	BTEST_EXPECT_EQUAL("%d", count(world, all), 2);
}

BTEST(query, foreach_with_mutation) {
	bent_world_t* world = fixture.world;

	for (int i = 0; i < 10; ++i) {
		bent_t ent = bent_create(world);
		bent_add(world, ent, basic_component, &i);
	}

	int visited = 0;
	BENT_FOREACH_MATCH(ent, world, BENT_COMP_LIST(&basic_component), NULL) {
		++visited;
		int value = *(int*)bent_get(world, ent, basic_component);
		// Entities that start matching inside the loop are not visited
		BTEST_EXPECT(value < 100);

		// Unmatch the current entity
		if (value % 2 == 0) { bent_remove(world, ent, basic_component); }

		// Match a new one
		bent_t fresh = bent_create(world);
		bent_add(world, fresh, basic_component, &(int){ 100 });
	}
	BTEST_EXPECT_EQUAL("%d", visited, 10);

	// 5 odd survivors and 10 fresh ones
	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 15);
}

BTEST(query, foreach_destroy_others) {
	bent_world_t* world = fixture.world;

	bent_t ents[8];
	for (int i = 0; i < 8; ++i) {
		ents[i] = bent_create(world);
		bent_add(world, ents[i], basic_component, NULL);
	}

	int visited = 0;
	BENT_FOREACH_MATCH(ent, world, BENT_COMP_LIST(&basic_component), NULL) {
		(void)ent;
		++visited;
		// Including the current one and the ones not yet visited
		for (int i = 0; i < 8; ++i) {
			bent_destroy(world, ents[i]);
		}
	}
	BTEST_EXPECT_EQUAL("%d", visited, 1);

	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);
}

BTEST(query, foreach_break_and_continue) {
	bent_world_t* world = fixture.world;

	for (int i = 0; i < 5; ++i) {
		bent_t ent = bent_create(world);
		bent_add(world, ent, basic_component, NULL);
	}

	int visited = 0;
	BENT_FOREACH_MATCH(ent, world, BENT_COMP_LIST(&basic_component), NULL) {
		(void)ent;
		++visited;
		if (visited == 2) { break; }
	}
	BTEST_EXPECT_EQUAL("%d", visited, 2);

	visited = 0;
	int after_continue = 0;
	BENT_FOREACH_MATCH(ent, world, BENT_COMP_LIST(&basic_component), NULL) {
		(void)ent;
		++visited;
		if (visited % 2 == 0) { continue; }
		++after_continue;
	}
	BTEST_EXPECT_EQUAL("%d", visited, 5);
	BTEST_EXPECT_EQUAL("%d", after_continue, 3);
}

BTEST(query, nested) {
	bent_world_t* world = fixture.world;

	for (int i = 0; i < 3; ++i) {
		bent_t ent = bent_create(world);
		bent_add(world, ent, basic_component, NULL);
	}

	int pairs = 0;
	BENT_FOREACH_MATCH(a, world, BENT_COMP_LIST(&basic_component), NULL) {
		(void)a;
		BENT_FOREACH_MATCH(b, world, BENT_COMP_LIST(&basic_component), NULL) {
			(void)b;
			++pairs;
			// The inner loop can also stop early
			if (pairs % 3 == 0) { break; }
		}
	}
	BTEST_EXPECT_EQUAL("%d", pairs, 9);

	// The outer loop is unaffected by the inner one's snapshots
	int outer = 0;
	BENT_FOREACH_MATCH(a, world, BENT_COMP_LIST(&basic_component), NULL) {
		++outer;
		BENT_FOREACH_MATCH(b, world, BENT_COMP_LIST(&basic_component), NULL) {
			if (!bent_equal(a, b)) { bent_destroy(world, b); }
		}
	}
	BTEST_EXPECT_EQUAL("%d", outer, 1);
}

BTEST(query, explicit_iterator) {
	bent_world_t* world = fixture.world;

	for (int i = 0; i < 4; ++i) {
		bent_t ent = bent_create(world);
		bent_add(world, ent, basic_component, NULL);
	}
	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);

	// Early exit
	bent_query_itr_t itr = bent_query_begin(world, q);
	BTEST_EXPECT(bent_query_next(world, &itr));
	BTEST_EXPECT(bent_query_match(world, q, itr.entity));
	bent_query_end(world, &itr);
	bent_query_end(world, &itr);  // Idempotent
	BTEST_EXPECT(!bent_query_next(world, &itr));

	// Full run
	int visited = 0;
	itr = bent_query_begin(world, q);
	while (bent_query_next(world, &itr)) {
		++visited;
	}
	BTEST_EXPECT_EQUAL("%d", visited, 4);

	// The empty query
	itr = bent_query_begin(world, (bent_query_t){ 0 });
	BTEST_EXPECT(!bent_query_next(world, &itr));
}

static void
count_entity(void* userdata, bent_world_t* world, bent_t entity) {
	(void)world;
	(void)entity;
	++*(int*)userdata;
}

BTEST(query, each) {
	bent_world_t* world = fixture.world;

	for (int i = 0; i < 3; ++i) {
		bent_t ent = bent_create(world);
		bent_add(world, ent, basic_component, NULL);
	}
	bent_create(world);

	int visited = 0;
	bent_query_each(
		world,
		bent_query(world, BENT_COMP_LIST(&basic_component), NULL),
		count_entity, &visited
	);
	BTEST_EXPECT_EQUAL("%d", visited, 3);
}

BTEST(query, shared_with_system) {
	bent_world_t* world = fixture.world;

	// A system's list is the same query anyone else would get
	bent_query_t q = bent_query(
		world,
		BENT_COMP_LIST(&basic_component),
		BENT_COMP_LIST(&basic_component2)
	);
	BTEST_EXPECT_EQUAL("%d", bent_sys_query(world, system_with_exclusion).id, q.id);

	// A system without a filter matches nothing
	BTEST_EXPECT_EQUAL("%d", bent_sys_query(world, dummy).id, 0);
	BTEST_EXPECT_EQUAL("%d", count(world, bent_sys_query(world, dummy)), 0);

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);

	BTEST_EXPECT_EQUAL("%d", count(world, bent_sys_query(world, system_with_exclusion)), 1);
	BTEST_EXPECT(query_yields(world, bent_sys_query(world, system_with_exclusion), ent));
	BTEST_EXPECT(bent_match(world, system_with_exclusion, ent));

	simple_system_t* sys = bent_get_sys_data(world, system_with_exclusion);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);

	// Mutating through a query iteration still drives the callbacks
	BENT_FOREACH_QUERY(e, world, q) {
		bent_add(world, e, basic_component2, NULL);
	}
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 1);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);
}

BTEST(query, survives_reload) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);
	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);

	// Reload
	bent_init(&world, NULL);
	BTEST_EXPECT(world == fixture.world);

	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT(bent_query_match(world, q, ent));
	bent_query_t q2 = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	BTEST_EXPECT_EQUAL("%d", q2.id, q.id);

	// Systems were not told twice
	simple_system_t* sys = bent_get_sys_data(world, system_with_exclusion);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);
}

BTEST(query, load) {
	bent_world_t* world = fixture.world;

	bent_t ent1 = bent_create(world);
	bent_add(world, ent1, basic_component, NULL);
	bent_t ent2 = bent_create(world);
	bent_add(world, ent2, basic_component, NULL);
	bent_add(world, ent2, basic_component2, NULL);

	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);
	bent_query_t excluding = bent_query(
		world,
		BENT_COMP_LIST(&basic_component),
		BENT_COMP_LIST(&basic_component2)
	);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 2);
	BTEST_EXPECT_EQUAL("%d", count(world, excluding), 1);

	// Save the handles
	bent_handles_t handles = bent_handles(world);
	bent_index_t* gens = malloc(sizeof(bent_index_t) * handles.len);
	memcpy(gens, handles.gens, sizeof(bent_index_t) * handles.len);
	handles.gens = gens;

	// ent2 matched until it got the excluded component
	simple_system_t* sys = bent_get_sys_data(world, system_with_exclusion);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 1);

	bent_clear(world);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);
	BTEST_EXPECT_EQUAL("%d", count(world, excluding), 0);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 2);

	// Load: the lists are current at every step, the callbacks wait
	bent_begin_load(world);
	BTEST_ASSERT(bent_load_handles(world, handles));
	free(gens);

	bent_restore(world, ent1, basic_component);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT_EQUAL("%d", count(world, excluding), 1);

	bent_restore(world, ent2, basic_component2);
	bent_restore(world, ent2, basic_component);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 2);
	BTEST_EXPECT_EQUAL("%d", count(world, excluding), 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 2);

	// A query created mid-load is populated too
	bent_query_t late = bent_query(world, BENT_COMP_LIST(&basic_component2), NULL);
	BTEST_EXPECT_EQUAL("%d", count(world, late), 1);

	bent_end_load(world);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 2);
	BTEST_EXPECT_EQUAL("%d", count(world, excluding), 1);
	BTEST_EXPECT_EQUAL("%d", count(world, late), 1);
	BTEST_EXPECT(bent_query_match(world, excluding, ent1));
	BTEST_EXPECT(!bent_query_match(world, excluding, ent2));

	// Exactly one add for ent1.
	// ent2 is loaded with both components at once so it never matched.
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 3);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 2);

	// No duplicate left behind by the replay
	bent_destroy(world, ent1);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 1);
	BTEST_EXPECT_EQUAL("%d", count(world, excluding), 0);
	bent_destroy(world, ent2);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);
	BTEST_EXPECT_EQUAL("%d", count(world, late), 0);
}

// A system that only wants callbacks must not intern a query
BENT_DECLARE_COMP(listless_comp)
BENT_DEFINE_COMP(listless_comp) = { .size = sizeof(int), .flags = BENT_COMP_RAW };
BENT_DECLARE_COMP(unused_comp)
BENT_DEFINE_COMP(unused_comp) = { .size = sizeof(int), .flags = BENT_COMP_RAW };

static void
listless_add(void* data, bent_world_t* world, bent_t entity) {
	(void)world;
	(void)entity;
	++((simple_system_t*)data)->num_adds;
}

static void
listless_remove(void* data, bent_world_t* world, bent_t entity) {
	(void)world;
	(void)entity;
	++((simple_system_t*)data)->num_removes;
}

BENT_DEFINE_SYS(listless_system) = {
	.size = sizeof(simple_system_t),
	.require = BENT_COMP_LIST(&listless_comp),
	.exclude = BENT_COMP_LIST(&basic_component2),
	.flags = BENT_SYS_NO_ENTITY_LIST,
	.add = listless_add,
	.remove = listless_remove,
};

BTEST(query, no_entity_list_creates_no_query) {
	bent_world_t* world = fixture.world;

	BTEST_EXPECT_EQUAL("%d", bent_sys_query(world, listless_system).id, 0);

	// Query ids are sequential: a filter nobody asked for yet gets the next id
	bent_query_t probe = bent_query(world, BENT_COMP_LIST(&unused_comp), NULL);
	bent_query_t same_filter = bent_query(
		world,
		BENT_COMP_LIST(&listless_comp),
		BENT_COMP_LIST(&basic_component2)
	);
	BTEST_EXPECT_EQUAL("%d", same_filter.id, probe.id + 1);

	// Still matched and called back like any other system
	simple_system_t* sys = bent_get_sys_data(world, listless_system);
	bent_t ent = bent_create(world);
	BTEST_EXPECT(!bent_match(world, listless_system, ent));

	bent_add(world, ent, listless_comp, NULL);
	BTEST_EXPECT(bent_match(world, listless_system, ent));
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 0);

	bent_add(world, ent, basic_component2, NULL);
	BTEST_EXPECT(!bent_match(world, listless_system, ent));
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 1);

	bent_remove(world, ent, basic_component2);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 2);

	bent_destroy(world, ent);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 2);

	// Never given a list
	BTEST_EXPECT_EQUAL("%d", count(world, bent_sys_query(world, listless_system)), 0);

	// A reload does not re-notify (and may move the system's data)
	bent_init(&world, NULL);
	sys = bent_get_sys_data(world, listless_system);
	bent_t ent2 = bent_create(world);
	bent_add(world, ent2, listless_comp, NULL);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 3);
	bent_init(&world, NULL);
	sys = bent_get_sys_data(world, listless_system);
	BTEST_EXPECT_EQUAL("%d", sys->num_adds, 3);
	BTEST_EXPECT_EQUAL("%d", sys->num_removes, 2);
}

BTEST(query, private_context) {
	bent_world_t* world = fixture.world;

	for (int i = 0; i < 6; ++i) {
		bent_t ent = bent_create(world);
		bent_add(world, ent, basic_component, NULL);
	}
	bent_query_t q = bent_query(world, BENT_COMP_LIST(&basic_component), NULL);

	bent_query_ctx_t* ctx = bent_create_query_ctx(NULL);

	// A private context nested inside the shared one, and the other way around
	int pairs = 0;
	BENT_FOREACH_QUERY(a, world, q) {
		(void)a;
		BENT_FOREACH_QUERY_EX(b, world, ctx, q) {
			(void)b;
			++pairs;
		}
	}
	BTEST_EXPECT_EQUAL("%d", pairs, 36);

	pairs = 0;
	BENT_FOREACH_QUERY_EX(a, world, ctx, q) {
		BENT_FOREACH_QUERY(b, world, q) {
			if (bent_equal(a, b)) { bent_destroy(world, b); }
			++pairs;
		}
	}
	// Each outer entity sees one fewer inner entity than the previous one
	BTEST_EXPECT_EQUAL("%d", pairs, 6 + 5 + 4 + 3 + 2 + 1);
	BTEST_EXPECT_EQUAL("%d", count(world, q), 0);

	// The explicit form on a private context, stopped early
	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);
	bent_query_itr_t itr = bent_query_begin_ex(world, q, ctx);
	BTEST_EXPECT(bent_query_next(world, &itr));
	bent_query_end(world, &itr);

	int visited = 0;
	bent_query_each_ex(world, q, ctx, count_entity, &visited);
	BTEST_EXPECT_EQUAL("%d", visited, 1);

	// NULL is the shared context
	itr = bent_query_begin_ex(world, q, NULL);
	BTEST_EXPECT(bent_query_next(world, &itr));
	BTEST_EXPECT(!bent_query_next(world, &itr));

	bent_destroy_query_ctx(ctx);
	bent_destroy_query_ctx(NULL);
}
