#include "../bent.h"
#include <assert.h>
#include <stdio.h>

// Note: Ignore the `//! [label]` markers, they are for for Doxygen
// They are pushed out of the way so you don't need to pay attention to them

//!                                                                             [BENT_DEFINE_COMP]
// Declare a component data type
typedef struct {
	float x, y;
	float rotation;
} transform_t;

// Register it
BENT_DEFINE_COMP(transform) = {  // This is a regular designated initializer
	.size = sizeof(transform_t),  // The most important property
	/* .init = ... */ // Optionally with callbacks
};
//!                                                                             [BENT_DEFINE_COMP]

//!                                                                             [BENT_DEFINE_POD_COMP]
// Let's define another component
typedef struct {
	int hp;
} health_t;

// Most components do not have init or cleanup so there is a shortcut
BENT_DEFINE_POD_COMP(health, health_t)
//!                                                                             [BENT_DEFINE_POD_COMP]

BENT_DEFINE_COMP_ADDER(health, health_t)
BENT_DEFINE_COMP_GETTER(health, health_t)

// A component can also be zero-sized, in that case it is just a tag

BENT_DEFINE_COMP(tag) = {
	.size = 0,
};

// Helper for adding this component since we don't really need an argument
static inline void
bent_add_tag(bent_world_t* world, bent_t entity) {
	bent_add(
		world,  // world
		entity, // entity
		tag,    // registration
		NULL    // argument
	);
}

// Let's define a system

// First, there can be multiple update phases
enum {
	PHASE_UPDATE = 1 << 0,
	PHASE_RENDER = 1 << 1,
};

//!                                                                             [BENT_DEFINE_SYS]
// Start with some callbacks

// Called during update
static void
health_bar_update(
	void* userdata,
	bent_world_t* world,
	bent_mask_t update_mask,
	bent_query_t query
) {
	// The matching entities are visited through the query
	BENT_FOREACH_QUERY(e, world, query) {
		(void)e;
	}
}

// Called when a component is added
static void
health_bar_add(void* userdata, bent_world_t* world, bent_t entity) {
}

// Register the system
BENT_DEFINE_SYS(health_bar) = {
	// Register the callbacks
	.update = health_bar_update,
	.add = health_bar_add,

	// Which components are required
	.require = BENT_COMP_LIST(&transform, &health),  // These must exist at compile time
	/* .exlude = ... */  // A system can also have exclusion criteria
	.update_mask = PHASE_RENDER,

	// A system may also have data
	/* .size = sizeof(...), */

	// That can be initalized
	/* .init = health_bar_init */

	// And cleaned upp
	/* .cleanup = health_bar_cleanup */
};
//!                                                                             [BENT_DEFINE_SYS]

// See it in action
int
main(int argc, const char* arg[]) {
	bent_world_t* world = NULL;

	// (Re)initialize a world which is a container for entities
	bent_init(&world, NULL);

	// Create an entity
	bent_t ent = bent_create(world);

	// Component can be added directly
	//!                                                                         [bent_add]
	bent_add(world, ent, transform, &(transform_t){
		.x = 10,
		.y = 11,
	});
	//!                                                                         [bent_add]

	// Or using a type-safe helper
	bent_add_health(world, ent, &(health_t){
		.hp = 999,
	});

	// Component can be retrieved:
	health_t* health_data = bent_get(world, ent, health);
	(void)health_data->hp;

	// Or checked for existence
	bent_has(world, ent, tag);  // false

	// Component can be removed
	bent_remove(world, ent, tag);
	// Removal of a non-existent component is noop

	// The same goes to double addition
	health_t* health_data2 = bent_add_health(world, ent, NULL);
	assert(health_data2 == health_data);  // The same instance as before

	// Update the world in phases
	bent_run(world, PHASE_UPDATE);
	bent_run(world, PHASE_RENDER);

	// Entities can also be visited without a system, through a query
	//!                                                                         [BENT_FOREACH_MATCH]
	BENT_FOREACH_MATCH(
		e, world,
		BENT_COMP_LIST(&transform, &health),  // Required
		BENT_COMP_LIST(&tag)                  // Excluded, may be NULL
	) {
		// The body may freely mutate the world, even the current entity
		if (bent_get_health(world, e)->hp <= 0) {
			bent_add_tag(world, e);  // Excluded from now on
		}
	}
	//!                                                                         [BENT_FOREACH_MATCH]

	// The query behind it is a handle that can be kept, e.g: in a system's data.
	// It is looked up by its masks so this returns the same query every time.
	bent_query_t alive = bent_query(
		world, BENT_COMP_LIST(&transform, &health), BENT_COMP_LIST(&tag)
	);

	// The explicit iterator is for when the loop has to stop early
	//!                                                                         [bent_query_begin]
	bent_query_itr_t itr = bent_query_begin(world, alive);
	while (bent_query_next(world, &itr)) {
		if (bent_get_health(world, itr.entity)->hp > 500) {
			bent_query_end(world, &itr);  // Required before leaving early
			break;
		}
	}
	//!                                                                         [bent_query_begin]

	// Let's destroy an entity
	bent_destroy(world, ent);

	// It is safe to interact with a stale handle
	assert(!bent_has(world, ent, tag));

	// Destroy everything
	bent_cleanup(&world);

	// Bonus chapter: "Reflection"
	// Tools can iterate through the list of known systems and component types
	//!                                                                         [BENT_FOREACH_COMP]
	BENT_FOREACH_COMP(itr) {
		printf(
			"Component %s has id: %d and size: %zu\n",
			itr.name, itr.comp.id, itr.comp.def->size
		);
	}
	//!                                                                         [BENT_FOREACH_COMP]
	//!                                                                         [BENT_FOREACH_SYS]
	BENT_FOREACH_SYS(itr) {
		printf(
			"System %s has id: %d and size: %zu\n",
			itr.name, itr.sys.id, itr.sys.def->size
		);
	}
	//!                                                                         [BENT_FOREACH_SYS]
}
