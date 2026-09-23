// This sample is a single file so it also implements the dual use
// declarations (BENT_MSG, BENT_POD_COMP...), see BENT_POD_COMP
#define BENT_DEFINE_COMPONENTS
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
BENT_DEFINE_COMP_ADDER(transform, transform_t)
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

// Systems can talk to each other through messages
//!                                                                             [BENT_MSG]
// A message is a plain struct.
// This defines `struct damage_msg`, the typedef `damage_msg_t` and the
// registration `damage_msg` whose address identifies the message type.
BENT_MSG(damage_msg) {
	bent_t source;
	int amount;
};

// A system receives a message by listing a handler for it.
// The handler is only called for entities the system matches.
static void
health_on_damage(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const damage_msg_t* damage = msg;  // The payload
	bent_get_health(world, entity)->hp -= damage->amount;
}

BENT_DEFINE_SYS(damage_taker) = {
	.require = BENT_COMP_LIST(&health),
	.handlers = BENT_MSG_HANDLERS(
		{ &damage_msg, health_on_damage }
	),
};
//!                                                                             [BENT_MSG]

// A message about no entity in particular is broadcast instead.
// Every system with a handler receives it, whatever it matches.
BENT_MSG(turn_msg) { int turn; };

static int last_turn = 0;

static void
on_turn(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	(void)entity;  // Invalid: there is no entity
	last_turn = ((const turn_msg_t*)msg)->turn;
}

BENT_DEFINE_SYS(turn_logger) = {
	.require = BENT_COMP_LIST(&health),  // Irrelevant for a broadcast
	.handlers = BENT_MSG_HANDLERS(
		{ &turn_msg, on_turn }
	),
};

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

	// An entity with several components can be created in one go.
	// Systems only hear about it once every component is there.
	//!                                                                         [bent_create_from]
	bent_t goblin = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(transform, { .x = 4, .y = 2 }),  // Typed through the adder helper
		BENT_COMP(health, { .hp = 30 }),
		BENT_COMP(tag)  // No argument for a tag
	));

	// A prefab can also be kept and created from repeatedly
	bent_prefab_t rat = BENT_PREFAB(
		BENT_COMP(transform),  // Zeroed
		BENT_COMP(health, { .hp = 3 })
	);
	bent_t rat1 = bent_create_from(world, rat);
	bent_t rat2 = bent_create_from(world, rat);

	// A prefab can include another one. The first entry for a component wins,
	// so what comes before the base overrides it and what comes after is a
	// default.
	bent_t big_rat = bent_create_from(world, BENT_PREFAB(
		BENT_COMP(health, { .hp = 9 }),  // Instead of the rat's 3
		BENT_PREFAB_BASE(rat),
		BENT_COMP(tag)  // The rat has none, so this one is added
	));
	//!                                                                         [bent_create_from]
	(void)goblin; (void)rat1; (void)rat2; (void)big_rat;

	// Update the world in phases
	bent_run(world, PHASE_UPDATE);
	bent_run(world, PHASE_RENDER);

	// Send a message to an entity.
	// Every system that handles it and matches the entity receives it.
	// The sender does not need to know which systems those are.
	//!                                                                         [bent_send]
	bent_send(world, ent, damage_msg, { .source = ent, .amount = 10 });
	assert(bent_get_health(world, ent)->hp == 989);

	// A message can also be built first, e.g: to send to both sides of a pair
	damage_msg_t splash = bent_msg(damage_msg){ .amount = 5 };
	bent_send(world, ent, damage_msg, splash);
	bent_send(world, ent, damage_msg, splash);
	assert(bent_get_health(world, ent)->hp == 979);
	//!                                                                         [bent_send]

	// Broadcast to every system that handles the message
	//!                                                                         [bent_broadcast]
	bent_broadcast(world, turn_msg, { .turn = 3 });
	assert(last_turn == 3);
	//!                                                                         [bent_broadcast]

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
