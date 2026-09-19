// Build with: cc main.c components.c movement.c
#define BLIB_IMPLEMENTATION
#include "components.h"
#include <assert.h>
#include <stdio.h>

// The serialization context is whatever the host wants.
// A real program would use a serialization library such as bsv.h.
// This one copies bytes in or out of a buffer.
struct sample_io_s {
	bool writing;
	size_t pos;
	unsigned char buf[256];
};

static bool
sample_io(sample_io_t* io, void* data, size_t size) {
	if (io->pos + size > sizeof(io->buf)) { return false; }

	if (io->writing) {
		memcpy(io->buf + io->pos, data, size);
	} else {
		memcpy(data, io->buf + io->pos, size);
	}
	io->pos += size;
	return true;
}

//!                                                                             [BENT_SERIALIZER]
// Expands to the head that components.h declared:
// `bool bent_serialize_target(bent_serialize_ctx_t* ctx, target_t* comp)`.
// The same function both saves and loads, the context knows the direction.
BENT_SERIALIZER(target) {
	// An entity handle can be saved as is
	return sample_io(ctx, &comp->entity, sizeof(comp->entity));
}

// The serializer of a read-only component still gets a mutable pointer: on
// load, it is the constructor
BENT_SERIALIZER(position) {
	return sample_io(ctx, &comp->x, sizeof(comp->x))
		&& sample_io(ctx, &comp->y, sizeof(comp->y));
}
//!                                                                             [BENT_SERIALIZER]

static bool
save(bent_world_t* world, sample_io_t* io) {
	// A component with data that made no choice cannot be saved
	if (bent_unserializable_comp() != NULL) { return false; }

	// The handles, so that a bent_t stored in a component stays valid
	bent_handles_t handles = bent_handles(world);
	if (!sample_io(io, &handles.len, sizeof(handles.len))) { return false; }
	for (bent_index_t i = 0; i < handles.len; ++i) {
		bent_index_t gen = handles.gens[i];
		if (!sample_io(io, &gen, sizeof(gen))) { return false; }
	}

	// Then for each component type: the entities that have it and their data
	BENT_FOREACH_SAVED_COMP(itr) {
		bent_index_t count = bent_count_with(world, itr.comp);
		if (!sample_io(io, &count, sizeof(count))) { return false; }

		BENT_FOREACH_WITH(entity, world, itr.comp) {
			if (!sample_io(io, &entity, sizeof(entity))) { return false; }

			// A tag is saved by presence alone
			if (bent_comp_save_mode(itr.comp.def) != BENT_COMP_SAVE_CALLBACK) { continue; }

			if (!itr.comp.def->serialize(io, bent_get(world, entity, itr.comp))) {
				return false;
			}
		}
	}

	return true;
}

static bool
load(bent_world_t* world, sample_io_t* io) {
	bent_index_t num_handles;
	if (!sample_io(io, &num_handles, sizeof(num_handles))) { return false; }
	bent_index_t* gens = bent_load_handles_begin(world, num_handles);
	if (gens == NULL) { return false; }
	bool handles_loaded = sample_io(io, gens, sizeof(*gens) * num_handles);
	bent_load_handles_end(world);
	if (!handles_loaded) { return false; }

	BENT_FOREACH_SAVED_COMP(itr) {
		bent_index_t count;
		if (!sample_io(io, &count, sizeof(count))) { return false; }

		for (bent_index_t i = 0; i < count; ++i) {
			bent_t entity;
			if (!sample_io(io, &entity, sizeof(entity))) { return false; }

			// Zeroed storage, no callback: the serializer is the constructor
			void* data = bent_restore(world, entity, itr.comp);
			if (data != NULL && !itr.comp.def->serialize(io, data)) { return false; }
		}
	}

	return true;
}

int
main(int argc, const char* argv[]) {
	(void)argc; (void)argv;

	bent_world_t* world = NULL;
	bent_init(&world, NULL);

	bent_t tower = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(position, { .x = 1, .y = 2 })
	));
	bent_t rat = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(position, { .x = 5, .y = 5 }),
		BENT_COMP(animation, { .frame = 3 })
	));
	// The typed helpers come from components.h
	bent_add_target(world, tower, &(target_t){ .entity = rat });

	// position is read-only here
	const position_t* pos = bent_get_position(world, rat);
	// pos->x += 1;  // Does not compile
	movement_move(world, rat, 1, -1);  // Ask the owner instead
	assert(pos->x == 6 && pos->y == 4);

	// Save
	sample_io_t io = { .writing = true };
	bool saved = save(world, &io);
	assert(saved);
	printf("Saved %zu bytes\n", io.pos);

	// Load into the same world, emptied
	bent_clear(world);
	bent_begin_load(world);  // Systems hear nothing until bent_end_load
	io.writing = false;
	io.pos = 0;
	bool loaded = load(world, &io);
	if (!loaded) { bent_clear(world); }  // Abandon a failed load
	bent_end_load(world);
	assert(loaded);

	// Handles are the same as before the save
	pos = bent_get_position(world, rat);
	assert(pos->x == 6 && pos->y == 4);
	assert(bent_equal(bent_get_target(world, tower)->entity, rat));
	// A transient component is lost
	assert(!bent_has(world, rat, animation));
	printf("Rat is at (%d, %d)\n", pos->x, pos->y);

	bent_cleanup(&world);
	return 0;
}
