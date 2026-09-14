#include "shared.h"
#include "../../btest.h"
#include <stdlib.h>

static btest_suite_t serialize = {
	.name = "bent/serialize",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

// A component referencing another entity: the whole point of keeping handles
typedef struct {
	bent_t target;
} link_t;

// The "stream" for the callback components: a growable byte buffer that is
// written in one direction and read back in the other
typedef struct {
	char* data;
	size_t len;
	size_t cursor;
	bool reading;
} stream_t;

static void
stream_io(stream_t* stream, void* value, size_t size) {
	if (stream->reading) {
		memcpy(value, stream->data + stream->cursor, size);
	} else {
		stream->data = realloc(stream->data, stream->len + size);
		memcpy(stream->data + stream->len, value, size);
		stream->len += size;
	}
	stream->cursor += size;
}

static int num_link_serializations = 0;

static bool
link_serialize(bent_serialize_ctx_t* ctx, void* data) {
	++num_link_serializations;
	stream_io(ctx, data, sizeof(link_t));
	return true;
}

BENT_DEFINE_COMP(link) = {
	.size = sizeof(link_t),
	.serialize = link_serialize,
};

BENT_DEFINE_COMP(position) = {
	.size = sizeof(float[2]),
	.flags = BENT_COMP_RAW,
};

BENT_DEFINE_TAG_COMP(marker)

static int num_cache_inits = 0;
static int num_cache_cleanups = 0;

static void
cache_init(void* data, void* arg) {
	(void)data; (void)arg;
	++num_cache_inits;
}

static void
cache_cleanup(void* data) {
	(void)data;
	++num_cache_cleanups;
}

// Derived data: the system below re-creates it from `position`
BENT_DEFINE_COMP(cache) = {
	.size = sizeof(int),
	.init = cache_init,
	.cleanup = cache_cleanup,
	.flags = BENT_COMP_TRANSIENT,
};

typedef struct {
	int num_adds;
	int num_removes;
	int total;  // Serialized
} counting_sys_t;

static void
counting_add(void* userdata, bent_world_t* world, bent_t entity) {
	(void)world; (void)entity;
	counting_sys_t* sys = userdata;
	++sys->num_adds;
}

static void
counting_remove(void* userdata, bent_world_t* world, bent_t entity) {
	(void)world; (void)entity;
	counting_sys_t* sys = userdata;
	++sys->num_removes;
}

static bool
counting_serialize(bent_serialize_ctx_t* ctx, void* data) {
	counting_sys_t* sys = data;
	stream_io(ctx, &sys->total, sizeof(sys->total));
	return true;
}

// Requires the callback component
BENT_DEFINE_SYS(link_sys) = {
	.size = sizeof(counting_sys_t),
	.require = BENT_COMP_LIST(&link),
	.add = counting_add,
	.remove = counting_remove,
	.serialize = counting_serialize,
};

// Adds a transient component from its add callback, like a renderer would
static void
cache_builder_add(void* userdata, bent_world_t* world, bent_t entity) {
	counting_add(userdata, world, entity);
	bent_add(world, entity, cache, NULL);
}

BENT_DEFINE_SYS(cache_builder) = {
	.size = sizeof(counting_sys_t),
	.require = BENT_COMP_LIST(&position),
	.add = cache_builder_add,
	.remove = counting_remove,
};

// Requires the transient component, so it can only be reached through the
// callback above
BENT_DEFINE_SYS(cache_user) = {
	.size = sizeof(counting_sys_t),
	.require = BENT_COMP_LIST(&cache),
	.add = counting_add,
	.remove = counting_remove,
};

// Excludes the transient component: added by position then removed again
// once the cache appears, exactly as at runtime
BENT_DEFINE_SYS(cache_hater) = {
	.size = sizeof(counting_sys_t),
	.require = BENT_COMP_LIST(&position),
	.exclude = BENT_COMP_LIST(&cache),
	.add = counting_add,
	.remove = counting_remove,
};

BTEST(serialize, save_mode) {
	bent_comp_def_t tag = { 0 };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&tag), BENT_COMP_SAVE_PRESENCE);

	bent_comp_def_t forgot = { .size = 4 };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&forgot), BENT_COMP_SAVE_INVALID);

	bent_comp_def_t raw = { .size = 4, .flags = BENT_COMP_RAW };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&raw), BENT_COMP_SAVE_RAW);

	bent_comp_def_t transient = { .size = 4, .flags = BENT_COMP_TRANSIENT };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&transient), BENT_COMP_SAVE_NONE);

	bent_comp_def_t transient_tag = { .flags = BENT_COMP_TRANSIENT };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&transient_tag), BENT_COMP_SAVE_NONE);

	bent_comp_def_t callback = { .size = 4, .serialize = link_serialize };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&callback), BENT_COMP_SAVE_CALLBACK);

	bent_comp_def_t both = { .size = 4, .flags = BENT_COMP_RAW, .serialize = link_serialize };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&both), BENT_COMP_SAVE_INVALID);

	bent_comp_def_t contradiction = { .size = 4, .flags = BENT_COMP_RAW | BENT_COMP_TRANSIENT };
	BTEST_EXPECT_EQUAL("%d", bent_comp_save_mode(&contradiction), BENT_COMP_SAVE_INVALID);

	// Every component in this test binary made a choice
	BTEST_EXPECT(bent_unserializable_comp() == NULL);

	int num_saved = 0;
	int num_total = 0;
	BENT_FOREACH_COMP(itr) { ++num_total; }
	BENT_FOREACH_SAVED_COMP(itr) {
		BTEST_EXPECT(bent_comp_save_mode(itr.comp.def) >= BENT_COMP_SAVE_PRESENCE);
		++num_saved;
	}
	BTEST_EXPECT(num_saved > 0);
	BTEST_EXPECT(num_saved < num_total);  // The transient ones are filtered out
}

BTEST(serialize, find_by_name) {
	BTEST_EXPECT(bent_find_comp("link").def == link.def);
	BTEST_EXPECT(bent_find_comp("position").def == position.def);
	BTEST_EXPECT(bent_find_comp("nope").def == NULL);
	BTEST_EXPECT(bent_find_sys("link_sys").def == link_sys.def);
	BTEST_EXPECT(bent_find_sys("nope").def == NULL);
}

BTEST(serialize, null_handle) {
	bent_world_t* world = fixture.world;

	bent_t invalid = { 0 };
	BTEST_EXPECT(bent_is_invalid(invalid));
	BTEST_EXPECT(!bent_is_invalid(bent_create(world)));
	BTEST_EXPECT(!bent_reserve(world, invalid));
}

BTEST(serialize, iterate) {
	bent_world_t* world = fixture.world;
	enum { N = 20 };
	bent_t entities[N];

	for (int i = 0; i < N; ++i) {
		entities[i] = bent_create(world);
		if (i % 2 == 0) { bent_add(world, entities[i], marker, NULL); }
	}
	for (int i = 0; i < N; i += 4) {
		bent_destroy(world, entities[i]);
	}

	int num_live = 0;
	bent_index_t last_index = 0;
	BENT_FOREACH_LIVE(entity, world) {
		BTEST_EXPECT(bent_is_active(world, entity));
		BTEST_EXPECT(num_live == 0 || entity.index > last_index);
		last_index = entity.index;
		++num_live;
	}
	BTEST_EXPECT_EQUAL("%d", num_live, N - N / 4);

	int num_marked = 0;
	BENT_FOREACH_WITH(entity, world, marker) {
		BTEST_EXPECT(bent_has(world, entity, marker));
		++num_marked;
	}
	BTEST_EXPECT_EQUAL("%d", num_marked, N / 2 - N / 4);
	BTEST_EXPECT_EQUAL("%d", (int)bent_count_with(world, marker), num_marked);
	BTEST_EXPECT_EQUAL("%d", (int)bent_count_with(world, link), 0);

	// Destroying the current entity mid-loop is safe
	BENT_FOREACH_LIVE(entity, world) {
		bent_destroy(world, entity);
	}
	num_live = 0;
	BENT_FOREACH_LIVE(entity, world) { (void)entity; ++num_live; }
	BTEST_EXPECT_EQUAL("%d", num_live, 0);
}

BTEST(serialize, clear) {
	bent_world_t* world = fixture.world;
	num_cache_cleanups = 0;

	bent_t a = bent_create(world);
	bent_t b = bent_create(world);
	bent_add(world, a, position, NULL);  // cache_builder adds a cache
	bent_add(world, b, cache, NULL);
	counting_sys_t* users = bent_get_sys_data(world, cache_user);
	BTEST_EXPECT_EQUAL("%d", users->num_adds, 2);

	bent_clear(world);
	BTEST_EXPECT(!bent_is_active(world, a));
	BTEST_EXPECT(!bent_is_active(world, b));
	BTEST_EXPECT_EQUAL("%d", num_cache_cleanups, 2);
	BTEST_EXPECT_EQUAL("%d", users->num_removes, 2);

	bent_index_t num_entities;
	bent_get_entity_list(world, cache_user, &num_entities);
	BTEST_EXPECT_EQUAL("%d", (int)num_entities, 0);

	// Old handles stay stale after the slots are reused
	bent_t c = bent_create(world);
	BTEST_EXPECT(bent_is_active(world, c));
	BTEST_EXPECT(!bent_is_active(world, a));
	BTEST_EXPECT(!bent_is_active(world, b));
}

BTEST(serialize, reserve) {
	bent_world_t* world = fixture.world;

	bent_t far = { .index = 1000, .gen = 7 };
	BTEST_EXPECT(bent_reserve(world, far));
	BTEST_EXPECT(bent_is_active(world, far));
	BTEST_EXPECT(!bent_has(world, far, marker));
	BTEST_EXPECT(bent_reserve(world, far));  // Idempotent

	bent_t conflict = { .index = 1000, .gen = 9 };
	BTEST_EXPECT(!bent_reserve(world, conflict));
	BTEST_EXPECT(bent_is_active(world, far));

	// A reserved entity is a regular entity
	bent_add(world, far, position, NULL);
	counting_sys_t* builder = bent_get_sys_data(world, cache_builder);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 1);
	BTEST_EXPECT(bent_has(world, far, cache));

	// Creating never hands out a reserved slot
	for (int i = 0; i < 2000; ++i) {
		bent_t entity = bent_create(world);
		BTEST_EXPECT(entity.index != far.index);
	}
	BTEST_EXPECT(bent_is_active(world, far));
}

BTEST(serialize, handles_round_trip) {
	bent_world_t* world = fixture.world;
	enum { N = 30 };
	bent_t entities[N];
	for (int i = 0; i < N; ++i) {
		entities[i] = bent_create(world);
	}
	for (int i = 0; i < N; i += 3) {
		bent_destroy(world, entities[i]);
	}
	bent_t reused = bent_create(world);

	// Copy the view: the world is about to change
	bent_handles_t handles = bent_handles(world);
	bent_index_t* gens = malloc(handles.len * sizeof(bent_index_t));
	memcpy(gens, handles.gens, handles.len * sizeof(bent_index_t));
	handles.gens = gens;

	bent_clear(world);
	for (int i = 0; i < N; ++i) {
		BTEST_EXPECT(!bent_is_active(world, entities[i]));
	}

	BTEST_ASSERT(bent_load_handles(world, handles));
	free(gens);

	for (int i = 0; i < N; ++i) {
		BTEST_EXPECT(bent_is_active(world, entities[i]) == (i % 3 != 0));
	}
	BTEST_EXPECT(bent_is_active(world, reused));

	// Loaded entities are empty and regular
	BTEST_EXPECT(!bent_has(world, entities[1], marker));
	bent_add(world, entities[1], marker, NULL);
	BTEST_EXPECT(bent_has(world, entities[1], marker));

	// New entities never collide and the stale ones never come back
	for (int i = 0; i < N; ++i) {
		bent_t entity = bent_create(world);
		for (int j = 1; j < N; j += 3) {
			BTEST_EXPECT(!bent_equal(entity, entities[j]));
		}
	}
	for (int i = 0; i < N; i += 3) {
		BTEST_EXPECT(!bent_is_active(world, entities[i]));
	}
}

// A complete save and load through the primitives, the way a host would do it
// with its serialization library
typedef struct {
	bent_index_t* gens;
	bent_index_t num_gens;
	bent_t* linked;
	int num_linked;
	bent_t* positioned;
	float (*positions)[2];
	int num_positioned;
	bent_t* marked;
	int num_marked;
	stream_t stream;
} save_t;

static void
save_world(bent_world_t* world, save_t* save) {
	BTEST_ASSERT(bent_unserializable_comp() == NULL);
	memset(save, 0, sizeof(*save));

	bent_handles_t handles = bent_handles(world);
	save->num_gens = handles.len;
	save->gens = malloc(handles.len * sizeof(bent_index_t));
	memcpy(save->gens, handles.gens, handles.len * sizeof(bent_index_t));

	save->linked = malloc(bent_count_with(world, link) * sizeof(bent_t));
	BENT_FOREACH_WITH(entity, world, link) {
		save->linked[save->num_linked++] = entity;
		link.def->serialize(&save->stream, bent_get(world, entity, link));
	}

	int num_positioned = (int)bent_count_with(world, position);
	save->positioned = malloc(num_positioned * sizeof(bent_t));
	save->positions = malloc(num_positioned * sizeof(*save->positions));
	BENT_FOREACH_WITH(entity, world, position) {
		save->positioned[save->num_positioned] = entity;
		memcpy(save->positions[save->num_positioned], bent_get(world, entity, position), position.def->size);
		++save->num_positioned;
	}

	save->marked = malloc(bent_count_with(world, marker) * sizeof(bent_t));
	BENT_FOREACH_WITH(entity, world, marker) {
		save->marked[save->num_marked++] = entity;
	}

	link_sys.def->serialize(&save->stream, bent_get_sys_data(world, link_sys));
}

static void
load_world(bent_world_t* world, save_t* save) {
	save->stream.reading = true;
	save->stream.cursor = 0;

	bent_clear(world);
	bent_begin_load(world);
	BTEST_ASSERT(bent_load_handles(world, (bent_handles_t){ .len = save->num_gens, .gens = save->gens }));

	for (int i = 0; i < save->num_linked; ++i) {
		void* data = bent_restore(world, save->linked[i], link);
		BTEST_ASSERT(data != NULL);
		link.def->serialize(&save->stream, data);
	}
	for (int i = 0; i < save->num_positioned; ++i) {
		void* data = bent_restore(world, save->positioned[i], position);
		BTEST_ASSERT(data != NULL);
		memcpy(data, save->positions[i], position.def->size);
	}
	for (int i = 0; i < save->num_marked; ++i) {
		BTEST_ASSERT(bent_is_active(world, save->marked[i]));
		BTEST_ASSERT(!bent_has(world, save->marked[i], marker));
		bent_restore(world, save->marked[i], marker);
	}

	link_sys.def->serialize(&save->stream, bent_get_sys_data(world, link_sys));
	bent_end_load(world);
}

static void
free_save(save_t* save) {
	free(save->gens);
	free(save->linked);
	free(save->positioned);
	free(save->positions);
	free(save->marked);
	free(save->stream.data);
}

BTEST(serialize, world_round_trip) {
	bent_world_t* world = fixture.world;
	num_cache_inits = 0;
	num_cache_cleanups = 0;
	num_link_serializations = 0;

	bent_t target = bent_create(world);
	bent_t source = bent_create(world);
	bent_t stale = bent_create(world);
	bent_t lone = bent_create(world);
	bent_add(world, source, link, &(link_t){ .target = target });
	bent_add(world, target, position, &(float[2]){ 1.0f, 2.0f });
	bent_add(world, target, marker, NULL);
	bent_add(world, lone, position, &(float[2]){ 3.0f, 4.0f });
	bent_add(world, stale, marker, NULL);
	bent_destroy(world, stale);
	bent_t reused = bent_create(world);  // Takes stale's slot
	BTEST_EXPECT_EQUAL("%u", reused.index, stale.index);

	counting_sys_t* links = bent_get_sys_data(world, link_sys);
	counting_sys_t* builder = bent_get_sys_data(world, cache_builder);
	counting_sys_t* users = bent_get_sys_data(world, cache_user);
	counting_sys_t* haters = bent_get_sys_data(world, cache_hater);
	links->total = 42;
	BTEST_EXPECT_EQUAL("%d", links->num_adds, 1);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", users->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", haters->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", haters->num_removes, 2);
	BTEST_EXPECT_EQUAL("%d", num_cache_inits, 2);

	save_t save;
	save_world(world, &save);
	BTEST_EXPECT_EQUAL("%d", save.num_linked, 1);
	BTEST_EXPECT_EQUAL("%d", save.num_positioned, 2);
	BTEST_EXPECT_EQUAL("%d", save.num_marked, 1);
	BTEST_EXPECT_EQUAL("%d", num_link_serializations, 1);

	// Wipe everything, including the system state
	links->total = 0;
	load_world(world, &save);
	free_save(&save);
	BTEST_EXPECT_EQUAL("%d", num_link_serializations, 2);
	BTEST_EXPECT_EQUAL("%d", num_cache_cleanups, 2);

	// Same handles, same data
	BTEST_EXPECT(bent_is_active(world, target));
	BTEST_EXPECT(bent_is_active(world, source));
	BTEST_EXPECT(bent_is_active(world, lone));
	BTEST_EXPECT(bent_is_active(world, reused));
	BTEST_EXPECT(!bent_is_active(world, stale));

	link_t* restored_link = bent_get(world, source, link);
	BTEST_ASSERT(restored_link != NULL);
	BTEST_EXPECT(bent_equal(restored_link->target, target));
	BTEST_EXPECT(bent_get(world, restored_link->target, position) != NULL);

	float* pos = bent_get(world, target, position);
	BTEST_ASSERT(pos != NULL);
	BTEST_EXPECT_EQUAL("%f", pos[0], 1.0f);
	BTEST_EXPECT_EQUAL("%f", pos[1], 2.0f);
	pos = bent_get(world, lone, position);
	BTEST_ASSERT(pos != NULL);
	BTEST_EXPECT_EQUAL("%f", pos[0], 3.0f);
	BTEST_EXPECT(bent_has(world, target, marker));
	BTEST_EXPECT(!bent_has(world, lone, marker));
	BTEST_EXPECT(!bent_has(world, reused, marker));  // Never saved for that generation

	// Systems saw each entity exactly once, with its complete component set,
	// and the transient component was rebuilt by the callback
	BTEST_EXPECT_EQUAL("%d", links->total, 42);
	BTEST_EXPECT_EQUAL("%d", links->num_adds, 2);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 4);
	BTEST_EXPECT_EQUAL("%d", users->num_adds, 4);
	BTEST_EXPECT_EQUAL("%d", haters->num_adds, 4);
	BTEST_EXPECT_EQUAL("%d", haters->num_removes, 4);
	BTEST_EXPECT_EQUAL("%d", num_cache_inits, 4);
	BTEST_EXPECT(bent_has(world, target, cache));
	BTEST_EXPECT(bent_has(world, lone, cache));

	bent_index_t num_entities;
	bent_get_entity_list(world, cache_user, &num_entities);
	BTEST_EXPECT_EQUAL("%d", (int)num_entities, 2);
	bent_get_entity_list(world, cache_hater, &num_entities);
	BTEST_EXPECT_EQUAL("%d", (int)num_entities, 0);
	bent_get_entity_list(world, link_sys, &num_entities);
	BTEST_EXPECT_EQUAL("%d", (int)num_entities, 1);

	// And it keeps working as a regular world
	bent_remove(world, target, position);
	BTEST_EXPECT_EQUAL("%d", builder->num_removes, 3);
	BTEST_EXPECT(bent_has(world, target, cache));  // The builder's remove callback leaves it, as at runtime
	bent_destroy(world, source);
	BTEST_EXPECT_EQUAL("%d", links->num_removes, 2);
}

BTEST(serialize, restore_rules) {
	bent_world_t* world = fixture.world;

	bent_t alive = bent_create(world);
	bent_t dead = bent_create(world);
	bent_destroy(world, dead);
	bent_handles_t handles = bent_handles(world);
	bent_index_t* gens = malloc(handles.len * sizeof(bent_index_t));
	memcpy(gens, handles.gens, handles.len * sizeof(bent_index_t));
	handles.gens = gens;

	bent_clear(world);
	bent_begin_load(world);
	BTEST_ASSERT(bent_load_handles(world, handles));
	free(gens);

	// Not alive: refused
	BTEST_EXPECT(bent_restore(world, dead, position) == NULL);
	BTEST_EXPECT(bent_restore(world, (bent_t){ 0 }, position) == NULL);

	// Alive: zeroed storage, no init, no system notified yet
	float* pos = bent_restore(world, alive, position);
	BTEST_ASSERT(pos != NULL);
	BTEST_EXPECT_EQUAL("%f", pos[0], 0.0f);
	BTEST_EXPECT(bent_has(world, alive, position));
	counting_sys_t* builder = bent_get_sys_data(world, cache_builder);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 0);

	// Twice: refused
	BTEST_EXPECT(bent_restore(world, alive, position) == NULL);

	// Reserve works inside a load as well, for files without handles
	bent_t extra = { .index = 500, .gen = 3 };
	BTEST_EXPECT(bent_reserve(world, extra));
	BTEST_EXPECT(bent_restore(world, extra, position) != NULL);

	bent_end_load(world);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 2);
	BTEST_EXPECT(bent_has(world, alive, cache));
	BTEST_EXPECT(bent_has(world, extra, cache));
}

BTEST(serialize, abandon_load) {
	bent_world_t* world = fixture.world;
	num_cache_cleanups = 0;

	bent_begin_load(world);
	bent_t entity = { .index = 3, .gen = 1 };
	BTEST_ASSERT(bent_reserve(world, entity));
	bent_restore(world, entity, position);
	bent_restore(world, entity, cache);

	// Something went wrong: wipe and end
	bent_clear(world);
	bent_end_load(world);
	BTEST_EXPECT(!bent_is_active(world, entity));
	BTEST_EXPECT_EQUAL("%d", num_cache_cleanups, 1);
	counting_sys_t* builder = bent_get_sys_data(world, cache_builder);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 0);
	BTEST_EXPECT_EQUAL("%d", builder->num_removes, 0);

	// Usable afterwards
	bent_t fresh = bent_create(world);
	bent_add(world, fresh, position, NULL);
	BTEST_EXPECT_EQUAL("%d", builder->num_adds, 1);
}
