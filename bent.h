// vim: set foldmethod=marker foldlevel=0:
#ifndef BENTITY_H
#define BENTITY_H

/**
 * @file
 * @brief Entity component system with automatic registration and hot reloading suppport.
 *
 * Components and systems are automatically registered with the help of @ref autolist.h.
 * When a hot reload solution such as [remodule](https://github.com/bullno1/remodule),
 * is used changes to component or system definitions will be detected and the
 * system will be updated automatically.
 *
 * @remarks Not all properites can be hot-reloaded.
 *     They will be clearly marked as such in the following sections.
 *
 * ## Serialization
 *
 * The library does not pick a format.
 * It exposes helpers so the host can use any serialization method:
 *
 * - @ref bent_handles and @ref bent_load_handles save and restore the entity
 *   handles, so a @ref bent_t stored inside a component is valid again after
 *   a load.
 * - @ref BENT_FOREACH_SAVED_COMP, @ref BENT_FOREACH_WITH and
 *   @ref bent_count_with enumerate what to write.
 * - @ref bent_begin_load, @ref bent_reserve, @ref bent_restore and
 *   @ref bent_end_load rebuild a world.
 *   Systems are not notified until @ref bent_end_load, so their callbacks only
 *   ever see complete entities.
 *
 * Each component type declares how it takes part through
 * @ref bent_comp_def_t::serialize, @ref BENT_COMP_RAW or @ref BENT_COMP_TRANSIENT.
 * A component with data that declares nothing is a mistake and
 * @ref bent_unserializable_comp reports it.
 * A tag component is saved by presence alone.
 */

#include "autolist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

#ifndef BENT_API
#define BENT_API
#endif

/*! Customizable index type */
#ifndef BENT_INDEX_TYPE
#define BENT_INDEX_TYPE uint32_t
#endif

/*! Customizable bitmask type */
#ifndef BENT_MASK_TYPE
#define BENT_MASK_TYPE uint32_t
#endif

/*! Customizable log function */
#ifndef BENT_LOG
#define BENT_LOG(...)
#endif

/*! Customizable assert function */
#ifndef BENT_ASSERT
#include <assert.h>
#define BENT_ASSERT assert
#endif

/**
 * Maximum number of component types.
 *
 * This must be defined at compile-time and is **not** hot reloadable.
 */
#ifndef BENT_MAX_NUM_COMPONENT_TYPES
#define BENT_MAX_NUM_COMPONENT_TYPES 32
#endif

/**
 * Type of the context passed to serialization callbacks.
 *
 * The library never touches it.
 * Set it to the serialization library's context type, e.g: `bsv_ctx_t`, so
 * that callbacks do not need a cast.
 */
#ifndef BENT_SERIALIZE_CTX
#define BENT_SERIALIZE_CTX void
#endif

/*! Number of words in a @ref bent_bitset_t */
#define BENT_BITSET_LEN ((BENT_MAX_NUM_COMPONENT_TYPES + sizeof(bent_mask_t) * CHAR_BIT - 1) / (sizeof(bent_mask_t) * CHAR_BIT))

/**
 * Forward-declare a component type.
 *
 * This should be used in a header file.
 * It will declare a variable of type @ref bent_comp_reg_t.
 *
 * @param NAME name of the component type
 */
#define BENT_DECLARE_COMP(NAME) \
	extern bent_comp_reg_t NAME;

/**
 * Define a type-safe helper function to retrieve a component.
 *
 * @param NAME name of the component type
 * @param TYPE type of the component's data
 */
#define BENT_DEFINE_COMP_GETTER(NAME, TYPE) \
	static inline TYPE* bent_get_##NAME(bent_world_t* world, bent_t entity) { \
		return bent_get(world, entity, NAME); \
   	}

/**
 * Define a type-safe helper function to add a component.
 *
 * @param NAME name of the component type
 * @param TYPE type of the component's data
 */
#define BENT_DEFINE_COMP_ADDER(NAME, TYPE) \
	BENT_DEFINE_COMP_ADDER_EX(NAME, TYPE, TYPE)

/**
 * Define a type-safe helper function to add a component.
 *
 * @param NAME name of the component type
 * @param COMP_TYPE type of the component's data
 * @param ARG_TYPE type of the constructor argument
 */
#define BENT_DEFINE_COMP_ADDER_EX(NAME, COMP_TYPE, ARG_TYPE) \
	static inline COMP_TYPE* bent_add_##NAME(bent_world_t* world, bent_t entity, ARG_TYPE* arg) { \
		return bent_add(world, entity, NAME, arg); \
   	}

/**
 * Define a type-safe helper function to add a tag component (zero-sized).
 *
 * @param NAME name of the component type
 */
#define BENT_DEFINE_TAG_COMP_ADDER(NAME) \
	static inline void bent_add_##NAME(bent_world_t* world, bent_t entity) { \
		bent_add(world, entity, NAME, NULL); \
	}

/**
 * Define a component type
 *
 * This must be followed with an initializer list for the type @ref bent_comp_def_t.
 *
 * @param NAME name of the component type
 *
 * Example:
 *
 * @snippet samples/bent.c BENT_DEFINE_COMP
 *
 * @hideinitializer
 */
#define BENT_DEFINE_COMP(NAME) \
	extern bent_comp_def_t BENT__COMP_DEF_NAME(NAME); \
	bent_comp_reg_t NAME = { .def = &BENT__COMP_DEF_NAME(NAME) }; \
	AUTOLIST_ADD_ENTRY(bent__components, NAME, NAME) \
	bent_comp_def_t BENT__COMP_DEF_NAME(NAME)

/**
 * Define a component type, whose data is a POD (plain old data)
 *
 * Also define the add and get helpers.
 *
 * @param NAME name of the component type
 * @param TYPE type of the component's data
 *
 * Example:
 *
 * @snippet samples/bent.c BENT_DEFINE_POD_COMP
 */
#define BENT_DEFINE_POD_COMP(NAME, TYPE) \
	BENT_DEFINE_COMP(NAME) = { .size = sizeof(TYPE) };

/**
 * Define a component type with zero size.
 */
#define BENT_DEFINE_TAG_COMP(NAME) \
	BENT_DEFINE_COMP(NAME) = { .size = 0 };

/**
 * Iterate over each component type.
 *
 * @param ITR name of the iterator variable of type @ref bent_comp_itr_t
 *
 * Example:
 * @snippet samples/bent.c BENT_FOREACH_COMP
 *
 * @hideinitializer
 */
#define BENT_FOREACH_COMP(ITR) \
	AUTOLIST_FOREACH(bent__itr, bent__components) \
		for ( \
			bent_comp_itr_t ITR = { \
				.name = bent__itr->name, \
				.comp = *(const bent_comp_reg_t*)bent__itr->value_addr, \
			}; \
			ITR.name != NULL; \
			ITR.name = NULL \
		)

/**
 * Iterate over each component type that appears in a save.
 *
 * That is every type whose bent_comp_save_mode() is at least
 * @ref BENT_COMP_SAVE_PRESENCE, in registration order.
 *
 * @param ITR name of the iterator variable of type @ref bent_comp_itr_t
 *
 * @hideinitializer
 */
#define BENT_FOREACH_SAVED_COMP(ITR) \
	BENT_FOREACH_COMP(ITR) \
		if (bent_comp_save_mode(ITR.comp.def) < BENT_COMP_SAVE_PRESENCE) {} else

/**
 * Forward-declare a system.
 *
 * This should be used in a header file.
 * It will declare a variable of type @ref bent_sys_reg_t.
 *
 * @param NAME name of the system
 */
#define BENT_DECLARE_SYS(NAME) \
	extern bent_sys_reg_t NAME;

/**
 * Define a system.
 *
 * This must be followed with an initializer list for the type @ref bent_sys_def_t.
 *
 * @param NAME name of the system
 *
 * Example:
 * @snippet samples/bent.c BENT_DEFINE_SYS
 *
 * @hideinitializer
 */
#define BENT_DEFINE_SYS(NAME) \
	extern bent_sys_def_t BENT__SYS_DEF_NAME(NAME); \
	bent_sys_reg_t NAME = { .def = &BENT__SYS_DEF_NAME(NAME) }; \
	AUTOLIST_ADD_ENTRY(bent__systems, NAME, NAME) \
	bent_sys_def_t BENT__SYS_DEF_NAME(NAME)

/**
 * Iterate over each system.
 *
 * @param ITR name of the iterator variable of type @ref bent_sys_itr_t
 *
 * Example:
 * @snippet samples/bent.c BENT_FOREACH_SYS
 *
 * @hideinitializer
 */
#define BENT_FOREACH_SYS(ITR) \
	AUTOLIST_FOREACH(bent__itr, bent__systems) \
		for ( \
			bent_sys_itr_t ITR = { \
				.name = bent__itr->name, \
				.sys = *(const bent_sys_reg_t*)bent__itr->value_addr, \
			}; \
			ITR.name != NULL; \
			ITR.name = NULL \
		)

/**
 * Helper for a null-terminated component list.
 *
 * To be used inside a @ref bent_sys_def_t.
 */
#define BENT_COMP_LIST(...) (bent_comp_reg_t*[]){ __VA_ARGS__, 0 }

/**
 * Helper to iterate entities.
 */
#define BENT_FOREACH_ENTITY(VAR, ENTITIES) \
	for ( \
		struct { bent_index_t index; char once; } bent__itr = { 0 }; \
		bent__itr.index < bent__entity_list_len(ENTITIES); \
		++bent__itr.index \
	) \
		for ( \
			bent_t VAR = (bent__itr.once = 1, (ENTITIES)[bent__itr.index]); \
			bent__itr.once; \
			bent__itr.once = 0 \
		)

/**
 * Iterate every live entity of a world, in index order.
 *
 * Destroying the current entity inside the loop is safe.
 * Entities created inside the loop may or may not be visited.
 *
 * @param VAR name of the variable of type @ref bent_t
 * @param WORLD the world
 *
 * @hideinitializer
 */
#define BENT_FOREACH_LIVE(VAR, WORLD) \
	for ( \
		bent_t VAR = bent__next_live((WORLD), 0); \
		!bent_is_invalid(VAR); \
		VAR = bent__next_live((WORLD), VAR.index + 1) \
	)

/**
 * Iterate every live entity that has a component, in index order.
 *
 * @param VAR name of the variable of type @ref bent_t
 * @param WORLD the world
 * @param COMP a component's registration handle
 *
 * @hideinitializer
 */
#define BENT_FOREACH_WITH(VAR, WORLD, COMP) \
	for ( \
		bent_t VAR = bent__next_with((WORLD), (COMP), 0); \
		!bent_is_invalid(VAR); \
		VAR = bent__next_with((WORLD), (COMP), VAR.index + 1) \
	)

#ifndef BENT_DEFINE_COMPONENTS

/**
 * Dual use helper for POD component.
 *
 * In a header file, it will forward-declare the component and define inline
 * helpers.
 *
 * In a single source file, define `BENT_DEFINE_COMPONENTS` and include this
 * header to implement the component registration.
 */
#define BENT_POD_COMP(NAME, TYPE) \
	BENT_DECLARE_COMP(NAME) \
	BENT_DEFINE_COMP_ADDER(NAME, TYPE) \
	BENT_DEFINE_COMP_GETTER(NAME, TYPE)

/// Same as @ref BENT_POD_COMP but the component is serialized as raw bytes
#define BENT_RAW_POD_COMP BENT_POD_COMP

/// Same as @ref BENT_POD_COMP but the component is never serialized
#define BENT_TRANSIENT_POD_COMP BENT_POD_COMP

/**
 * Dual use helper for tag component.
 *
 * In a header file, it will forward-declare the component and define inline
 * helpers.
 *
 * In a single source file, define `BENT_DEFINE_COMPONENTS` and include this
 * header to implement the component registration.
 */
#define BENT_TAG_COMP(NAME) \
	BENT_DECLARE_COMP(NAME) \
	BENT_DEFINE_TAG_COMP_ADDER(NAME)

#else

#define BENT_POD_COMP(NAME, TYPE) BENT_DEFINE_POD_COMP(NAME, TYPE)

#define BENT_RAW_POD_COMP(NAME, TYPE) BENT_DEFINE_RAW_COMP(NAME, TYPE)

#define BENT_TRANSIENT_POD_COMP(NAME, TYPE) BENT_DEFINE_TRANSIENT_COMP(NAME, TYPE)

#define BENT_TAG_COMP(NAME) BENT_DEFINE_TAG_COMP(NAME)

#endif

/*! Handle to an entity world */
typedef struct bent_world_s bent_world_t;

/*! Index type used in the library */
typedef BENT_INDEX_TYPE bent_index_t;

/*! Bitmask type used in the library */
typedef BENT_MASK_TYPE bent_mask_t;

/*! A component bitset */
typedef struct {
	/// @cond INTERNAL
	bent_mask_t bits[BENT_BITSET_LEN];
	/// @endcond
} bent_bitset_t;

/**
 * Entity handle
 *
 * Each handle has a generation counter so using a stale handle belonging to
 * a destroyed entity is safe.
 *
 * An entity handle can also be zero-initialized.
 * In which case, it will be considered stale.
 *
 * Handles survive a save and load unchanged.
 *
 * Functions will behave in a sensible way when given a stale handle:
 *
 * * (Redudnant) destruction of the entity as well as addition or removal of components become noop.
 * * The destroyed entity is considered to contain no components.
 * * Data retrieval just return `NULL`.
 * * The destroyed entity will never @ref bent_match "match" any system,
 *   even those without any @ref bent_sys_def_t::require "requirements".
 */
typedef struct {
	/// @cond INTERNAL
	bent_index_t index;
	bent_index_t gen;
	/// @endcond
} bent_t;

/*! Serialization context, see @ref BENT_SERIALIZE_CTX */
typedef BENT_SERIALIZE_CTX bent_serialize_ctx_t;

/**
 * Serialization callback for a component or a system.
 *
 * Called in both directions, the direction is known to the context.
 * On read, `data` is zeroed storage and this callback is the constructor:
 * @ref bent_comp_def_t::init is not called.
 *
 * @param ctx the context the host's serializer passes along
 * @param data the component's or system's data
 * @return whether serialization succeeded
 */
typedef bool (*bent_serialize_fn_t)(bent_serialize_ctx_t* ctx, void* data);

/**
 * Component behaviour flags.
 *
 * @see bent_comp_def_t::flags
 */
typedef enum {
	/**
	 * Data is saved and restored as raw bytes.
	 *
	 * The size is checked on read.
	 * This is for plain structs on a single platform: there is no versioning
	 * and no endianness handling.
	 */
	BENT_COMP_RAW       = 1 << 0,

	/**
	 * Data is neither saved nor restored.
	 *
	 * Either a system's @ref bent_sys_def_t::add "add callback" re-creates
	 * it on load, or it is simply lost.
	 */
	BENT_COMP_TRANSIENT = 1 << 1,
} bent_comp_flags_t;

/**
 * How a component type takes part in a save.
 *
 * @see bent_comp_save_mode()
 */
typedef enum {
	/*! Has data but made no choice, or made contradicting ones. Saving must fail. */
	BENT_COMP_SAVE_INVALID,
	/*! Not in the file at all, see @ref BENT_COMP_TRANSIENT */
	BENT_COMP_SAVE_NONE,
	/*! A tag: the list of entities that have it */
	BENT_COMP_SAVE_PRESENCE,
	/*! The list of entities, each with a size-checked blob, see @ref BENT_COMP_RAW */
	BENT_COMP_SAVE_RAW,
	/*! The list of entities, each followed by whatever @ref bent_comp_def_t::serialize writes */
	BENT_COMP_SAVE_CALLBACK,
} bent_comp_save_t;

/**
 * Saved entity handles, a view into the world.
 *
 * One generation per slot, an odd generation is a live entity.
 * Valid until the next call that creates or destroys an entity.
 *
 * @see bent_handles
 * @see bent_load_handles
 */
typedef struct {
	/*! Number of slots */
	bent_index_t len;
	/*! One generation per slot */
	const bent_index_t* gens;
} bent_handles_t;

/**
 * Component type definition.
 *
 * @see BENT_DEFINE_COMP
 */
typedef struct {
	/**
	 * Size of the data for this component.
	 *
	 * If not specified it will be treated as 0.
	 * In which case, no extra storage is allocated.
	 * This component can be used purely for "tagging".
	 *
	 * @remarks This property is **not** hot reloadable.
	 */
	size_t size;

	/**
	 * Optional initialization callback for this component.
	 *
	 * If this is omitted, the default behavior is as follow:
	 *
	 * * If `arg` is `NULL`, the component's data will be zero-ed.
	 * * If `arg` is not `NULL`, `arg` will be `memcpy`-ed into the component.
	 *
	 * @param comp the component data.
	 * @param arg the argument passed to @ref bent_add
	 */
	void (*init)(void* comp, void* arg);

	/**
	 * Optional initialization callback for this component.
	 *
	 * If this is omitted, nothing will be done upon destruction.
	 * Take note that the component's storage memory is owned by the library and
	 * it can be recycled.
	 */
	void (*cleanup)(void* comp);

	/**
	 * Component behaviour flags.
	 *
	 * A bitwise OR of values from @ref bent_comp_flags_t.
	 */
	uint32_t flags;

	/**
	 * Optional serialization callback.
	 *
	 * A component with data must have exactly one of this, @ref BENT_COMP_RAW
	 * or @ref BENT_COMP_TRANSIENT, see bent_comp_save_mode().
	 */
	bent_serialize_fn_t serialize;
} bent_comp_def_t;

/**
 * A handle to a component's registration.
 *
 * @see BENT_DECLARE_COMP
 */
typedef struct {
	/*! Reference to the corresponding component definition */
	const bent_comp_def_t* def;

#ifndef DOXYGEN
	bent_index_t id;
#endif
} bent_comp_reg_t;

/**
 * System behavior flags.
 *
 * @see bent_sys_def_t::flags.
 */
typedef enum {
	/**
	 * Whether this system's @ref bent_sys_def_t::init callback can be called
	 * on reload.
	 */
	BENT_SYS_ALLOW_REINIT      = 1 << 0,

	/**
	 * When set, this system will not keep an internal entity list.
	 *
	 * Functions @ref bent_get_entity_list will return NULL.
	 */
	BENT_SYS_NO_ENTITY_LIST    = 1 << 1,
} bent_sys_flags_t;

/**
 * System definition.
 *
 * @see BENT_DEFINE_SYS
 */
typedef struct {
	/**
	 * Size of the data for this system.
	 *
	 * If not specified it will be treated as 0.
	 * In which case, no extra storage is allocated.
	 *
	 * @remarks This property is **not** hot reloadable.
	 */
	size_t size;

	/**
	 * The update mask for this system.
	 *
	 * Defaults to 0.
	 * In which case, @ref bent_run will never invoke this system's @ref bent_sys_def_t::update.
	 *
	 * @see bent_run
	 */
	bent_mask_t update_mask;

	/**
	 * Null-terminated list of required components.
	 *
	 * Defaults to an empty list which will match all entities.
	 *
	 * An entity must have all of the listed components to be processed by this system.
	 *
	 * @see BENT_COMP_LIST
	 *
	 * @remarks If both this and @ref bent_sys_def_t::exclude are `NULL`, this system
	 * will not match any entities.
	 */
	bent_comp_reg_t** require;

	/**
	 * Null-terminated list of required components.
	 *
	 * Defaults to an empty list which will match all entities.
	 *
	 * An entity must have none of the listed components to be processed by this system.
	 *
	 * @see BENT_COMP_LIST
	 *
	 * @remarks If both this and @ref bent_sys_def_t::require are `NULL`, this system
	 * will not match any entities.
	 */
	bent_comp_reg_t** exclude;

	/**
	 * System behaviour flags.
	 *
	 * A bitwise OR of values from @ref bent_sys_flags_t.
	 */
	uint32_t flags;

	/**
	 * Optional initialization callback.
	 *
	 * This may be called multiple times if @ref bent_sys_def_t::flags contains @ref BENT_SYS_ALLOW_REINIT.
	 *
	 * @param userdata pointer to a data buffer with the size given in @ref bent_sys_def_t::size
	 * @param world the world this system belongs to
	 *
	 * @see bent_init
	 */
	void (*init)(void* userdata, bent_world_t* world);

	/**
	 * Optional post-initialization callback.
	 *
	 * Unlike bent_sys_def_t::init, this will always be called after every system has finished initialization.
	 * This allows a dependent system to make queries to another system.
	 * The @ref BENT_SYS_ALLOW_REINIT flag has no effect.
	 *
	 * @param userdata pointer to a data buffer with the size given in @ref bent_sys_def_t::size
	 * @param world the world this system belongs to
	 *
	 * @see bent_init
	 */
	void (*post_init)(void* userdata, bent_world_t* world);

	/**
	 * Optional cleanup callback.
	 *
	 * @param userdata pointer to a data buffer with the size given in @ref bent_sys_def_t::size
	 * @param world the world this system belongs to
	 *
	 * @see bent_cleanup
	 */
	void (*cleanup)(void* userdata, bent_world_t* world);

	/**
	 * Optional addition callback.
	 *
	 * This will be called when an entity matches the criteria given in
	 * @ref bent_sys_def_t::require and @ref bent_sys_def_t::exclude.
	 *
	 * @param userdata system's data
	 * @param world the world this system belongs to
	 * @param entity id of the matching entity
	 *
	 * @see bent_add
	 * @see bent_remove
	 */
	void (*add)(void* userdata, bent_world_t* world, bent_t entity);

	/**
	 * Optional addition callback.
	 *
	 * This will be called when an entity no longer matches the criteria given in
	 * @ref bent_sys_def_t::require and @ref bent_sys_def_t::exclude or when
	 * a matching entity is destroyed.
	 *
	 * @param userdata system's data
	 * @param world the world this system belongs to
	 * @param entity id of the matching entity
	 *
	 * @see bent_add
	 * @see bent_remove
	 * @see bent_destroy
	 */
	void (*remove)(void* userdataata, bent_world_t* world, bent_t entity);

	/**
	 * Optional update callback.
	 *
	 * This will be called by @ref bent_run when a matching update mask is provided.
	 *
	 * @param userdata system's data
	 * @param world the world this system belongs to
	 * @param update_mask the value passed to @ref bent_run
	 * @param entities an array of entities
	 * @param num_entities number of entities in the array
	 *
	 * @see bent_run
	 */
	void (*update)(
		void* userdata,
		bent_world_t* world,
		bent_mask_t update_mask,
		bent_t* entities,
		bent_index_t num_entities
	);

	/**
	 * Optional serialization callback for the system's data.
	 *
	 * Unlike components, a system without one is simply not saved: its data
	 * is assumed to be derived and rebuilt in @ref bent_sys_def_t::init.
	 */
	bent_serialize_fn_t serialize;
} bent_sys_def_t;

/**
 * A handle to a system's registration.
 *
 * All members should be considered opaque.
 */
typedef struct {
	/*! Reference to the corresponding system definition */
	const bent_sys_def_t* def;

#ifndef DOXYGEN
	bent_index_t id;
#endif
} bent_sys_reg_t;

/**
 * A system iterator.
 *
 * @see BENT_FOREACH_SYS
 */
typedef struct {
	/*! Name of the system */
	const char* name;
	/*! The registration handle */
	bent_sys_reg_t sys;
} bent_sys_itr_t;

/**
 * A component type iterator.
 *
 * @see BENT_FOREACH_COMP
 */
typedef struct {
	/*! Name of the component type */
	const char* name;
	/*! The registration handle */
	bent_comp_reg_t comp;
} bent_comp_itr_t;

/**
 * Intialize an entity world.
 *
 * This function may be called multiple times in the case of hot reloading.
 * All callbacks and system's or component's properties will be automatically
 * updated.
 *
 * @param world_ptr pointer to a world
 * @param memctx memory allocator context
 * @return Whether this is the first initialization.
 *     The application can used the value to load initial data.
 */
BENT_API bool
bent_init(bent_world_t** world_ptr, void* memctx);

/**
 * Clean up an entity world.
 *
 * All entities will be destroyed and all memory will be freed.
 * Calling this on a `NULL` pointer is safe.
 *
 * @param world_ptr pointer to a world
 */
BENT_API void
bent_cleanup(bent_world_t** world_ptr);

/**
 * Retrieves the memory context of a world.
 *
 * This allows systems to use the same memory allocator as the world.
 *
 * @param world the world
 */
BENT_API void*
bent_memctx(bent_world_t* world);

/**
 * Create a new empty entity
 *
 * @param world the world
 * @return a new entity handle
 *
 * @see bent_t
 */
BENT_API bent_t
bent_create(bent_world_t* world);

/**
 * Destroy an existing entity
 *
 * If this is called during an @ref bent_run "update", the destruction will be
 * deferred until the update has finished.
 * If this is called from a system callback, the destruction will be deferred
 * until that callback has finished.
 *
 * @param world the world
 * @param entity an entity handle
 *
 * @see bent_is_active
 */
BENT_API void
bent_destroy(bent_world_t* world, bent_t entity);

/**
 * Check whether an entity is flagged for destruction
 *
 * @param world the world
 * @param entity an entity handle
 * @return whether this entity is flagged for destruction
 *
 * @see bent_destroy
 */
BENT_API bool
bent_is_active(bent_world_t* world, bent_t entity);

/**
 * Add a component to an entity
 *
 * If the entity already has this component, this is noop.
 * The existing component data is returned.
 *
 * @param world the world
 * @param entity an entity handle
 * @param comp a component's registration handle
 * @param arg argument to pass to @ref bent_comp_def_t::init
 * @return component data
 *
 * @remarks The returned pointer remains valid until the component is removed
 *     from the entity or the entity is destroyed.
 *     Component storage is never relocated.
 *
 * Example:
 *
 * @snippet samples/bent.c bent_add
 *
 * @see BENT_DEFINE_COMP
 * @see BENT_DECLARE_COMP
 * @see BENT_DEFINE_COMP_ADDER
 */
BENT_API void*
bent_add(bent_world_t* world, bent_t entity, bent_comp_reg_t comp, void* arg);

/**
 * Remove a component from an entity
 *
 * If the entity does not have this component, this is noop.
 *
 * @param world the world
 * @param entity an entity handle
 * @param comp a component's registration handle
 *
 * @see BENT_DEFINE_COMP
 * @see BENT_DECLARE_COMP
 */
BENT_API void
bent_remove(bent_world_t* world, bent_t entity, bent_comp_reg_t comp);

/**
 * Retrieve a component's data from an entity
 *
 * @param world the world
 * @param entity an entity handle
 * @param comp a component's registration handle
 * @return component's data or `NULL` if the entity does not have this component
 *
 * @remarks If the component has a zero size, this will always return `NULL`.
 *     For "tag" components, use @ref bent_has instead.
 *     The returned pointer remains valid until the component is removed
 *     from the entity or the entity is destroyed.
 *
 * @see BENT_DEFINE_COMP_GETTER
 */
BENT_API void*
bent_get(bent_world_t* world, bent_t entity, bent_comp_reg_t comp);

/**
 * Check whether an entity has a component
 *
 * @param world the world
 * @param entity an entity handle
 * @param comp a component's registration handle
 * @return whether the entity has the component
 */
BENT_API bool
bent_has(bent_world_t* world, bent_t entity, bent_comp_reg_t comp);

/**
 * Retrieve a system's private data
 *
 * Typically, this should only be called by the same system.
 *
 * @param world the world
 * @param sys a system's registration handle
 * @return the system's private data
 *
 * @see bent_sys_def_t::init
 * @see bent_sys_def_t::size
 */
BENT_API void*
bent_get_sys_data(bent_world_t* world, bent_sys_reg_t sys);

/**
 * Retrieve a system's name
 *
 * @param world the world
 * @param sys a system's registration handle
 * @return the system's name
 */
BENT_API const char*
bent_get_sys_name(bent_world_t* world, bent_sys_reg_t sys);

/**
 * Check whther a system may process an entity
 *
 * @param world the world
 * @param sys a system's registration handle
 * @param entity an entity handle
 * @return whether the entity matches a system's requirement
 *
 * @see bent_sys_def_t::require
 * @see bent_sys_def_t::exclude
 */
BENT_API bool
bent_match(bent_world_t* world, bent_sys_reg_t sys, bent_t entity);

/**
 * Run all systems matching the update mask
 *
 * A system can be registered with an @ref bent_sys_def_t::update_mask "update mask"
 * along with an @ref bent_sys_def_t::update "update callback".
 * When this function is called, only the systems whose update mask shares at
 * least a single bit with the given `update_mask` will have its update callback
 * invoked.
 * This allow the application to selectively call a subset of systems and force
 * an order among them.
 *
 * For example, it might be desirable to perform all logic updates first before rendering.
 * In this case, one may set `LOGIC_MASK = 1 << 0` and `RENDER_MASK = 1 << 1`.
 * `bent_run(world, LOGIC_MASK)` will only invoke systems whose `update_mask` contains this bit.
 * The same goes for `bent_run(world, RENDER_MASK)`.
 *
 * A system whose `update_mask = LOGIC_MASK | RENDER_MASK` will be invoked in both cases.
 *
 * @param world the world
 * @param update_mask an update mask
 *
 * @see bent_sys_def_t::update_mask
 * @see bent_sys_def_t::update
 */
BENT_API void
bent_run(bent_world_t* world, bent_mask_t update_mask);

/**
 * Retrieve the list of matched entities for a system
 *
 * @param world the world
 * @param sys the system
 * @param num_entities pointer to receive number of entities
 * @return The list of matched entities
 */
BENT_API bent_t*
bent_get_entity_list(bent_world_t* world, bent_sys_reg_t sys, bent_index_t* num_entities);

/**
 * Retrieve the component mask of an entity
 *
 * @param world the world
 * @param entity the entity
 * @return The component mask
 */
BENT_API bent_bitset_t
bent_get_entity_mask(bent_world_t* world, bent_t entity);

// Serialization support {{{

/**
 * Destroy every entity, running all cleanup callbacks.
 *
 * Must not be called from within @ref bent_run.
 *
 * @param world the world
 */
BENT_API void
bent_clear(bent_world_t* world);

/**
 * The state of every entity handle, see @ref bent_handles_t.
 *
 * @param world the world
 */
BENT_API bent_handles_t
bent_handles(bent_world_t* world);

/**
 * Replace the entity handles of an empty world with saved ones.
 *
 * Every entity that was alive at save time is alive again, empty, with the
 * same handle.
 * Every handle that was stale stays stale.
 * Outside of a load, systems that match an empty entity are notified.
 *
 * @param world the world, must have no entity, see @ref bent_clear
 * @param handles the saved handles
 * @return whether memory could be allocated
 */
BENT_API bool
bent_load_handles(bent_world_t* world, bent_handles_t handles);

/**
 * Zero-copy variant of @ref bent_load_handles.
 *
 * Returns a buffer of @p len generations for the host to fill, then
 * @ref bent_load_handles_end must be called.
 * Returns `NULL` if memory could not be allocated.
 *
 * @param world the world, must have no entity
 * @param len number of slots
 */
BENT_API bent_index_t*
bent_load_handles_begin(bent_world_t* world, bent_index_t len);

/*! Finish a @ref bent_load_handles_begin */
BENT_API void
bent_load_handles_end(bent_world_t* world);

/**
 * Make a specific handle alive, as an empty entity.
 *
 * For loading a file that was saved without @ref bent_handles.
 * Does nothing if the handle is already alive.
 * Fails if the handle is null, if its slot is taken by another generation or
 * if memory could not be allocated.
 *
 * Without the saved handles, the generations of slots that were free at save
 * time are lost, so a stale handle stored in the file can come back to life
 * once its slot is reused.
 *
 * @param world the world
 * @param entity the handle
 * @return whether the entity is alive
 */
BENT_API bool
bent_reserve(bent_world_t* world, bent_t entity);

/**
 * Begin loading a world.
 *
 * Until @ref bent_end_load, systems are not notified of anything.
 * This lets the loader restore an entity one component at a time while
 * every system callback still only sees complete entities.
 *
 * @param world the world, must have no entity, see @ref bent_clear
 */
BENT_API void
bent_begin_load(bent_world_t* world);

/**
 * Finish loading a world.
 *
 * Every entity is matched against every system as if it had just been
 * created with all of its components.
 * Callbacks invoked from here may add components to the entity being matched
 * but must not create entities.
 *
 * To abandon a failed load, call @ref bent_clear then this.
 *
 * @param world the world
 */
BENT_API void
bent_end_load(bent_world_t* world);

/**
 * Add a component to an entity without initializing it.
 *
 * Only valid between @ref bent_begin_load and @ref bent_end_load.
 * The entity must be alive, through @ref bent_load_handles or
 * @ref bent_reserve, and must not have the component yet.
 * The storage is zeroed and no callback runs: the caller fills it.
 *
 * @param world the world
 * @param entity an entity handle
 * @param comp a component's registration handle
 * @return the component's data, or `NULL` for a tag component or when the
 *     entity is not alive or already has the component
 */
BENT_API void*
bent_restore(bent_world_t* world, bent_t entity, bent_comp_reg_t comp);

/**
 * Number of live entities that have a component.
 *
 * This walks every entity.
 *
 * @param world the world
 * @param comp a component's registration handle
 */
BENT_API bent_index_t
bent_count_with(bent_world_t* world, bent_comp_reg_t comp);

/**
 * Find a component type by its registered name.
 *
 * @return the registration handle, with a `NULL` def if there is none
 */
BENT_API bent_comp_reg_t
bent_find_comp(const char* name);

/**
 * Find a system by its registered name.
 *
 * @return the registration handle, with a `NULL` def if there is none
 */
BENT_API bent_sys_reg_t
bent_find_sys(const char* name);

/**
 * Name of the first component type whose bent_comp_save_mode() is
 * @ref BENT_COMP_SAVE_INVALID, or `NULL` if there is none.
 *
 * A serializer should check this before saving.
 */
BENT_API const char*
bent_unserializable_comp(void);

/*! How a component type takes part in a save, see @ref bent_comp_save_t */
static inline bent_comp_save_t
bent_comp_save_mode(const bent_comp_def_t* def) {
	int raw = (def->flags & BENT_COMP_RAW) != 0;
	int transient = (def->flags & BENT_COMP_TRANSIENT) != 0;
	int callback = def->serialize != NULL;
	if (raw + transient + callback > 1) { return BENT_COMP_SAVE_INVALID; }
	if (transient) { return BENT_COMP_SAVE_NONE; }
	if (callback) { return BENT_COMP_SAVE_CALLBACK; }
	if (raw) { return BENT_COMP_SAVE_RAW; }
	if (def->size == 0) { return BENT_COMP_SAVE_PRESENCE; }
	return BENT_COMP_SAVE_INVALID;
}

// }}}

/*! Check whether two entity handles are equal */
static inline bool
bent_equal(bent_t lhs, bent_t rhs) {
	return lhs.index == rhs.index && lhs.gen == rhs.gen;
}

/*! Check whether a handle is invalid */
static inline bool
bent_is_invalid(bent_t entity) {
	return (entity.gen & 1) == 0;
}

// bitset {{{

/*! Clear all bits in a bitset */
static void
bent_bitset_clear(bent_bitset_t* bitset) {
	memset(bitset, 0, sizeof(*bitset));
}

/*! Set a bit in a bitset */
static void
bent_bitset_set(bent_bitset_t* bitset, bent_index_t bit_index) {
	bent_index_t num_bits_per_mask = sizeof(bent_index_t) * CHAR_BIT;
	bent_index_t mask_index = bit_index / num_bits_per_mask;
	bent_mask_t mask = (bent_mask_t)1 << (bit_index % num_bits_per_mask);
	bitset->bits[mask_index] |= mask;
}

/*! Unset a bit in a bitset */
static void
bent_bitset_unset(bent_bitset_t* bitset, bent_index_t bit_index) {
	bent_index_t num_bits_per_mask = sizeof(bent_index_t) * CHAR_BIT;
	bent_index_t mask_index = bit_index / num_bits_per_mask;
	bent_mask_t mask = ~((bent_mask_t)1 << (bit_index % num_bits_per_mask));
	bitset->bits[mask_index] &= mask;
}

/*! Flip all bits in a bitset */
static void
bent_bitset_flip(bent_bitset_t* bitset) {
	for (bent_index_t i = 0; i < BENT_BITSET_LEN; ++i) {
		bitset->bits[i] = ~bitset->bits[i];
	}
}

/*! Check whether a bit is set in a bitset */
static bool
bent_bitset_check(const bent_bitset_t* bitset, bent_index_t bit_index) {
	bent_index_t num_bits_per_mask = sizeof(bent_index_t) * CHAR_BIT;
	bent_index_t mask_index = bit_index / num_bits_per_mask;
	bent_mask_t mask = (bent_mask_t)1 << (bit_index % num_bits_per_mask);
	return (bitset->bits[mask_index] & mask) > 0;
}

/*! Check whether a bitset has at least one bit of another set */
static bool
bent_bitset_any_match(const bent_bitset_t* subject, const bent_bitset_t* requirement) {
	bool result = false;
	for (bent_index_t i = 0; i < BENT_BITSET_LEN; ++i) {
		bent_mask_t subject_mask = subject->bits[i];
		bent_mask_t required_mask = requirement->bits[i];
		result = result || ((subject_mask & required_mask) > 0);
	}
	return result;
}

/*! Check whether a bitset has all the bits of another set */
static bool
bent_bitset_all_match(const bent_bitset_t* subject, const bent_bitset_t* requirement) {
	bool result = true;
	for (bent_index_t i = 0; i < BENT_BITSET_LEN; ++i) {
		bent_mask_t subject_mask = subject->bits[i];
		bent_mask_t required_mask = requirement->bits[i];
		result = result && ((subject_mask & required_mask) == required_mask);
	}
	return result;
}

/*! Build a bitset from a NULL-terminated list of components */
static inline bent_bitset_t
bent_bitset_from_comp_list(bent_comp_reg_t** comp_list) {
	bent_bitset_t result = { 0 };
	for (
		bent_comp_reg_t** comp = comp_list;
		comp != NULL && *comp != NULL;
		++comp
	) {
		bent_bitset_set(&result, (*comp)->id - 1);
	}
	return result;
}

// }}}

// Private

#ifndef DOXYGEN

#define BENT__COMP_DEF_NAME(NAME) bent_comp_def_##NAME
#define BENT__SYS_DEF_NAME(NAME) bent_sys_def_##NAME

AUTOLIST_DECLARE(bent__components)
AUTOLIST_DECLARE(bent__systems)

BENT_API bent_index_t
bent__entity_list_len(bent_t* entities);

BENT_API bent_t
bent__next_live(bent_world_t* world, bent_index_t from);

BENT_API bent_t
bent__next_with(bent_world_t* world, bent_comp_reg_t comp, bent_index_t from);

#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BENT_IMPLEMENTATION)
#define BENT_IMPLEMENTATION
#endif

#ifdef BENT_IMPLEMENTATION

#ifndef BENT_REALLOC
#	ifdef BLIB_REALLOC
#		define BENT_REALLOC BLIB_REALLOC
#	else
#		define BENT_REALLOC(ptr, size, ctx) bent__libc_realloc(ptr, size, ctx)
#		define BENT_USE_LIBC_REALLOC
#	endif
#endif

#ifdef BENT_USE_LIBC_REALLOC
#include <stdlib.h>

static inline void*
bent__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

#define BARRAY_REALLOC BENT_REALLOC

// We depend on barray and bseg but try to hide their symbols to avoid conflict.
// If they are compiled in the same unit, honor the existing symbol decision.

#ifndef BARRAY_IMPLEMENTATION
#define BARRAY_API static inline
#define BARRAY_IMPLEMENTATION
#include "barray.h"
#endif

#define BSEG_REALLOC BENT_REALLOC

#ifndef BSEG_IMPLEMENTATION
#define BSEG_API static inline
#define BSEG_IMPLEMENTATION
#include "bseg.h"
#endif

#define BHANDLE_REALLOC BENT_REALLOC

#ifndef BHANDLE_IMPLEMENTATION
#ifndef BHANDLE_INDEX_TYPE
#define BHANDLE_INDEX_TYPE BENT_INDEX_TYPE
#endif
#define BHANDLE_API static inline
#define BHANDLE_IMPLEMENTATION
#include "bhandle.h"
#endif

// bent_t is a bhandle_t with the fields copied, so the two must agree
_Static_assert(
	sizeof(bhandle_index_t) == sizeof(bent_index_t)
	&& sizeof(bhandle_gen_t) == sizeof(bent_index_t),
	"BHANDLE_INDEX_TYPE and BHANDLE_GEN_TYPE must match BENT_INDEX_TYPE"
);

static inline bhandle_t
bent__to_bhandle(bent_t entity) {
	return (bhandle_t){ .index = entity.index, .gen = entity.gen };
}

static inline bent_t
bent__from_bhandle(bhandle_t handle) {
	return (bent_t){ .index = handle.index, .gen = handle.gen };
}

AUTOLIST_IMPL(bent__components)
AUTOLIST_IMPL(bent__systems)

typedef struct {
	bent_t entity;
	// Diff the two, or if `created`, match against systems that accept an
	// empty entity
	bent_bitset_t old_components;
	bent_bitset_t new_components;
	bool created;
} bent_notification_t;

typedef struct {
	bent_bitset_t require;
	bent_bitset_t exclude;
	barray(bent_index_t) sparse;
	barray(bent_t) dense;
	const bent_sys_def_t* def;
	char* name;
	void* userdata;
	bool initialized;
} bent_system_data_t;

typedef struct {
	// Segmented so that component pointers are stable
	bseg_t instances;
	const bent_comp_def_t* def;
	char* name;
} bent_component_data_t;

typedef struct {
	bent_bitset_t components;
	bool destroy_later;
} bent_entity_data_t;

struct bent_world_s {
	void* memctx;
	bool defer_destruction;
	// Between bent_begin_load and bent_end_load: systems are not notified
	bool loading;
	// A system callback is running: further notifications are queued so that
	// every callback sees a membership consistent with what it was told
	bool notifying;
	bool draining_destroy_queue;

	barray(bent_system_data_t) systems;
	barray(bent_notification_t) notify_queue;
	// Which slots are alive, and their generations
	bhandle_mgr_t handles;
	// One per slot, segmented so that entity data pointers stay valid across
	// user callbacks
	bseg(bent_entity_data_t) entities;
	barray(bent_t) destroy_queue;
	bent_component_data_t components[BENT_MAX_NUM_COMPONENT_TYPES];
	bent_index_t num_components;
};

static char*
bent_strcpy(const char* str, void* memctx) {
	size_t len = strlen(str);
	char* copy = BENT_REALLOC(NULL, len + 1, memctx);
	memcpy(copy, str, len + 1);
	return copy;
}

// component {{{

// Instance of a component at an index that has already been allocated
static void*
bent_comp_instance(bent_component_data_t* comp, bent_index_t index) {
	size_t size = comp->def->size;
	return size != 0 ? bseg__at(&comp->instances, index, size) : NULL;
}

// Instance of a component at an index, allocating storage if needed
static void*
bent_comp_ensure_instance(
	bent_component_data_t* comp,
	bent_index_t index,
	void* memctx
) {
	size_t size = comp->def->size;
	if (size == 0) { return NULL; }

	if (index >= bseg__capacity(&comp->instances)) {
		bseg__do_reserve(&comp->instances, (size_t)index + 1, size, memctx);
	}
	return bseg__at(&comp->instances, index, size);
}

static void
bent_comp_init(
	bent_world_t* world,
	bent_component_data_t* comp,
	const char* name, const bent_comp_def_t* def
) {
	comp->def = def;
	if (comp->name == NULL) {
		comp->name = bent_strcpy(name, world->memctx);
	}
}

static void
bent_comp_cleanup(
	bent_world_t* world,
	bent_component_data_t* comp
) {
	bseg__do_free(&comp->instances, world->memctx);
	BENT_REALLOC(comp->name, 0, world->memctx);
}

// }}}

// system {{{

typedef bool (*bhash_eq_fn_t)(const void* lhs, const void* rhs, size_t size);

static bool
bent_begin_notify(bent_world_t* world);

static void
bent_end_notify(bent_world_t* world);

static bool
bent_sys_match_impl(const bent_system_data_t* sys, const bent_bitset_t* components) {
	return bent_bitset_all_match(components, &sys->require)  // Match all of the requirements
		&& !bent_bitset_any_match(components, &sys->exclude);  // Match none of the exclusions
}

static void
bent_sys_add_entity(bent_world_t* world, bent_system_data_t* sys, bent_t entity) {
	if (!(sys->def->flags & BENT_SYS_NO_ENTITY_LIST)) {
		bent_index_t sparse_size = (bent_index_t)barray_len(sys->sparse);
		if (entity.index >= sparse_size) {
			bent_index_t new_sparse_size = sparse_size * 2 > entity.index + 1 ? sparse_size * 2 : entity.index + 1;
			barray_resize(sys->sparse, new_sparse_size, world->memctx);
		}
		sys->sparse[entity.index] = (bent_index_t)barray_len(sys->dense);
		barray_push(sys->dense, entity, world->memctx);
	}

	if (sys->def->add) {
		sys->def->add(sys->userdata, world, entity);
	}
}

static void
bent_sys_remove_entity(bent_world_t* world, bent_system_data_t* sys, bent_t entity) {
	if (!(sys->def->flags & BENT_SYS_NO_ENTITY_LIST)) {
		bent_index_t dense_index = sys->sparse[entity.index];
		bent_t last_entity = barray_pop(sys->dense);
		sys->dense[dense_index] = last_entity;
		sys->sparse[last_entity.index] = dense_index;
	}

	if (sys->def->remove) {
		sys->def->remove(sys->userdata, world, entity);
	}
}

static void
bent_sys_init(
	bent_world_t* world,
	bent_system_data_t* sys,
	const char* name, const bent_sys_def_t* def
) {
	sys->def = def;

	BENT_LOG("Initializing %s", name);

#ifndef BENT_NO_RELOAD
	// Backup old filter to detect change
	bent_bitset_t old_require = sys->require;
	bent_bitset_t old_exclude = sys->exclude;
#endif

	if (sys->name == NULL) {
		sys->name = bent_strcpy(name, world->memctx);
	}

	bent_bitset_clear(&sys->require);
	bent_bitset_clear(&sys->exclude);

	if (def->require == NULL && def->exclude == NULL) {
		// A system that specifies nothing will not match anything
		bent_bitset_flip(&sys->require);
		bent_bitset_flip(&sys->exclude);
	} else {
		// Otherwise, each property is defaulted to an empty list
		sys->require = bent_bitset_from_comp_list(def->require);
		sys->exclude = bent_bitset_from_comp_list(def->exclude);
	}

	bool initialized = sys->initialized;
	if (def->size) {
		sys->userdata = BENT_REALLOC(sys->userdata, def->size, world->memctx);
		if (!initialized) {
			memset(sys->userdata, 0, def->size);
		}
	}

	if (def->init && (!initialized || (def->flags & BENT_SYS_ALLOW_REINIT))) {
		def->init(sys->userdata, world);
	}

#ifndef BENT_NO_RELOAD
	// Update component inclusion based on new filter
	// Also, allow newly created systems to immediately register existing entities
	bent_system_data_t old_sys = {
		.require = old_require,
		.exclude = old_exclude,
	};
	bool outermost = bent_begin_notify(world);
	BHANDLE_FOREACH(handle, &world->handles) {
		const bent_entity_data_t* entity = bseg_ref(world->entities, handle.index);
		const bent_bitset_t* components = &entity->components;
		bent_t entity_id = bent__from_bhandle(handle);

		if (sys->initialized) {  // Existing system, do diff
			if (bent_sys_match_impl(&old_sys, components)) {
				if (!bent_sys_match_impl(sys, components)) {
					bent_sys_remove_entity(world, sys, entity_id);
				}
			} else {
				if (bent_sys_match_impl(sys, components)) {
					bent_sys_add_entity(world, sys, entity_id);
				}
			}
		} else {  // Newly registered system, try to match existing entities
			if (bent_sys_match_impl(sys, components)) {
				bent_sys_add_entity(world, sys, entity_id);
			}
		}
	}
	if (outermost) { bent_end_notify(world); }
#endif

	sys->initialized = true;
}

static void
bent_sys_cleanup(bent_world_t* world, bent_system_data_t* sys) {
	BENT_LOG("Cleaning up %s", sys->name);

	if (sys->def->cleanup) {
		sys->def->cleanup(sys->userdata, world);
	}
	BENT_REALLOC(sys->userdata, 0, world->memctx);
	barray_free(sys->dense, world->memctx);
	barray_free(sys->sparse, world->memctx);

	BENT_REALLOC(sys->name, 0, world->memctx);
}

static void
bent_notify_systems_impl(
	bent_world_t* world,
	bent_t entity,
	const bent_bitset_t* old_components,
	const bent_bitset_t* new_components
) {
	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t i = 0; i < num_systems; ++i) {
		bent_system_data_t* sys = &world->systems[i];
		if (bent_sys_match_impl(sys, old_components)) {
			if (!bent_sys_match_impl(sys, new_components)) {
				bent_sys_remove_entity(world, sys, entity);
			}
		} else {
			if (bent_sys_match_impl(sys, new_components)) {
				bent_sys_add_entity(world, sys, entity);
			}
		}
	}
}

// Systems that match an empty entity get every new entity.
// This is for completeness sake and also for consistent reload behavior.
static void
bent_match_empty_impl(bent_world_t* world, bent_t entity_id) {
	bent_bitset_t empty = { 0 };
	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t i = 0; i < num_systems; ++i) {
		bent_system_data_t* sys = &world->systems[i];
		if (bent_sys_match_impl(sys, &empty)) {
			bent_sys_add_entity(world, sys, entity_id);
		}
	}
}

static void
bent_dispatch_notification(bent_world_t* world, const bent_notification_t* notification) {
	if (notification->created) {
		bent_match_empty_impl(world, notification->entity);
	} else {
		bent_notify_systems_impl(
			world,
			notification->entity,
			&notification->old_components,
			&notification->new_components
		);
	}
}

static void
bent_drain_destroy_queue(bent_world_t* world);

// Returns whether this is the outermost notification.
// A nested one must be queued instead, see bent_notify.
static bool
bent_begin_notify(bent_world_t* world) {
	if (world->notifying) { return false; }
	world->notifying = true;
	return true;
}

// Deliver whatever the callbacks queued, then the entities they destroyed
static void
bent_end_notify(bent_world_t* world) {
	// Callbacks may queue more while draining
	for (bent_index_t i = 0; i < (bent_index_t)barray_len(world->notify_queue); ++i) {
		bent_notification_t notification = world->notify_queue[i];
		bent_dispatch_notification(world, &notification);
	}
	barray_clear(world->notify_queue);
	world->notifying = false;

	if (!world->defer_destruction) {
		bent_drain_destroy_queue(world);
	}
}

// While loading, systems are only told about entities in bent_end_load.
// While a callback runs, the notification waits until it is done.
static void
bent_notify(bent_world_t* world, bent_notification_t notification) {
	if (world->loading) { return; }

	if (!bent_begin_notify(world)) {
		barray_push(world->notify_queue, notification, world->memctx);
		return;
	}

	bent_dispatch_notification(world, &notification);
	bent_end_notify(world);
}

static void
bent_notify_systems(
	bent_world_t* world,
	bent_t entity,
	const bent_bitset_t* old_components,
	const bent_bitset_t* new_components
) {
	bent_notify(world, (bent_notification_t){
		.entity = entity,
		.old_components = *old_components,
		.new_components = *new_components,
	});
}

static void
bent_match_empty(bent_world_t* world, bent_t entity_id) {
	bent_notify(world, (bent_notification_t){
		.entity = entity_id,
		.created = true,
	});
}

// }}}

static bent_entity_data_t*
bent_entity_data(bent_world_t* world, bent_t entity_id) {
	ptrdiff_t index = bhandle_index(&world->handles, bent__to_bhandle(entity_id));
	if (index < 0) { return NULL; }

	return bseg_ref(world->entities, (size_t)index);
}

// Fresh entity data for a slot that was just made alive
static void
bent_reset_entity_data(bent_world_t* world, bent_index_t index) {
	bent_index_t capacity = bhandle_capacity(&world->handles);
	if (bseg_len(world->entities) < capacity) {
		bseg_resize(world->entities, capacity, world->memctx);
	}

	bent_entity_data_t* entity_data = bseg_ref(world->entities, index);
	*entity_data = (bent_entity_data_t){ 0 };
}

static void
bent_destroy_immediately(bent_world_t* world, bent_t entity_id) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return; }

	const bent_bitset_t* components = &entity_data->components;

	// While loading, no system has seen the entity yet
	if (!world->loading) {
		bool outermost = bent_begin_notify(world);
		bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
		for (bent_index_t i = 0; i < num_systems; ++i) {
			bent_system_data_t* sys = &world->systems[i];
			if (bent_sys_match_impl(sys, components)) {
				bent_sys_remove_entity(world, sys, entity_id);
			}
		}
		if (outermost) { bent_end_notify(world); }
	}

	bent_index_t num_components = world->num_components;
	for (bent_index_t i = 0; i < num_components; ++i) {
		bent_component_data_t* comp = &world->components[i];
		if (
			bent_bitset_check(components, i)
			&&
			comp->def->cleanup
		) {
			comp->def->cleanup(bent_comp_instance(comp, entity_id.index));
		}
	}

	bhandle_destroy(&world->handles, bent__to_bhandle(entity_id));
}

bool
bent_init(bent_world_t** world_ptr, void* memctx) {
	bent_world_t* world = *world_ptr;
	bool first_init = world == NULL;
	if (world == NULL) {
		world = BENT_REALLOC(NULL, sizeof(bent_world_t), memctx);
		*world = (bent_world_t){
			.memctx = memctx,
		};
	}

	AUTOLIST_FOREACH(itr, bent__components) {
		bent_comp_reg_t* reg = itr->value_addr;
#ifndef BENT_NO_RELOAD
		if (reg->id == 0) {  // Unregistered or we just reloaded
			// Search existing components for a match by name
			bent_index_t num_components = world->num_components;
			for (bent_index_t i = 0; i < num_components; ++i) {
				const bent_component_data_t* comp = &world->components[i];
				if (strcmp(comp->name, itr->name) == 0) {
					reg->id = i + 1;
					break;
				}
			}
		}
#endif

		// Still not found, register for the first time
		if (reg->id == 0) {
			reg->id = ++world->num_components;  // 1-based
			BENT_ASSERT(world->num_components <= BENT_MAX_NUM_COMPONENT_TYPES);
		}

		world->num_components = reg->id > world->num_components ? reg->id : world->num_components;
		bent_comp_init(
			world,
			&world->components[reg->id - 1],
			itr->name, reg->def
		);
	}

	// Count the number of systems first
	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	AUTOLIST_FOREACH(itr, bent__systems) {
		bent_sys_reg_t* reg = itr->value_addr;
#ifndef BENT_NO_RELOAD
		if (reg->id == 0) {  // Unregistered or we just reloaded
			// Search existing systems for a match by name
			for (bent_index_t i = 0; i < (bent_index_t)barray_len(world->systems); ++i) {
				const bent_system_data_t* sys = &world->systems[i];
				if (strcmp(sys->name, itr->name) == 0) {
					reg->id = i + 1;
					break;
				}
			}
		}
#endif

		// Still not found, register for the first time
		if (reg->id == 0) {
			reg->id = ++num_systems;
		}

		num_systems = num_systems > reg->id ? num_systems : reg->id;
	}

	// Do a single alloc
	if (num_systems > (bent_index_t)barray_len(world->systems)) {
		barray_resize(world->systems, num_systems, world->memctx);
	}

	// (Re)initialize systems
	AUTOLIST_FOREACH(itr, bent__systems) {
		bent_sys_reg_t* reg = itr->value_addr;

		bent_sys_init(
			world,
			&world->systems[reg->id - 1],
			itr->name, reg->def
		);
	}

	// Post initialization
	AUTOLIST_FOREACH(itr, bent__systems) {
		bent_sys_reg_t* reg = itr->value_addr;

		bent_system_data_t* sys = &world->systems[reg->id - 1];
		if (sys->def->post_init) {
			sys->def->post_init(sys->userdata, world);
		}
	}

	*world_ptr = world;
	return first_init;
}

void
bent_cleanup(bent_world_t** world_ptr) {
	bent_world_t* world = *world_ptr;
	if (world == NULL) { return; }

#ifndef BENT_NO_RELOAD
	// Edge case: cleaning up right after a reload
	bent_init(world_ptr, world->memctx);
	world = *world_ptr;
#endif

	bent_clear(world);

	bent_index_t num_components = world->num_components;
	for (bent_index_t i = 0; i < num_components; ++i) {
		bent_comp_cleanup(world, &world->components[i]);
	}

	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t i = 0; i < num_systems; ++i) {
		bent_sys_cleanup(world, &world->systems[i]);
	}

	barray_free(world->systems, world->memctx);
	barray_free(world->notify_queue, world->memctx);
	bseg_free(world->entities, world->memctx);
	bhandle_free(&world->handles, world->memctx);
	barray_free(world->destroy_queue, world->memctx);
	BENT_REALLOC(world, 0, world->memctx);

	*world_ptr = NULL;
}

void*
bent_memctx(bent_world_t* world) {
	return world->memctx;
}

bent_t
bent_create(bent_world_t* world) {
	bhandle_t handle = bhandle_new(&world->handles, world->memctx);
	BENT_ASSERT(!bhandle_is_null(handle));
	bent_reset_entity_data(world, handle.index);

	bent_t entity_id = bent__from_bhandle(handle);
	bent_match_empty(world, entity_id);
	return entity_id;
}

static void
bent_drain_destroy_queue(bent_world_t* world) {
	if (world->draining_destroy_queue) { return; }
	world->draining_destroy_queue = true;

	// Check queue length every iteration since destruction could lead to more
	// destruction
	for (bent_index_t i = 0; i < (bent_index_t)barray_len(world->destroy_queue); ++i) {
		bent_t entity = world->destroy_queue[i];
		bent_destroy_immediately(world, entity);
	}
	barray_clear(world->destroy_queue);

	world->draining_destroy_queue = false;
}

void
bent_destroy(bent_world_t* world, bent_t entity_id) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return; }

	if (world->defer_destruction || world->notifying) {
		if (!entity_data->destroy_later) {
			barray_push(world->destroy_queue, entity_id, world->memctx);
			entity_data->destroy_later = true;
		}
	} else {
		bent_destroy_immediately(world, entity_id);
	}
}

bool
bent_is_active(bent_world_t* world, bent_t entity_id) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	return bent_entity_data(world, entity_id) != NULL && !entity_data->destroy_later;
}

void*
bent_add(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg, void* arg) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return NULL; }

	bent_index_t comp_index = reg.id - 1;
	bent_component_data_t* comp_data = &world->components[comp_index];

	if (!bent_bitset_check(&entity_data->components, comp_index)) {
		// New component
		void* instance = bent_comp_ensure_instance(
			comp_data, entity_id.index, world->memctx
		);
		if (comp_data->def->init) {
			comp_data->def->init(instance, arg);
		} else if (instance != NULL) {
			if (arg == NULL) {
				memset(instance, 0, comp_data->def->size);
			} else {
				memcpy(instance, arg, comp_data->def->size);
			}
		}

		bent_bitset_t old_components = entity_data->components;
		bent_bitset_set(&entity_data->components, comp_index);
		bent_bitset_t new_components = entity_data->components;
		bent_notify_systems(world, entity_id, &old_components, &new_components);

		return instance;
	} else {
		// Already added, return existing data
		return bent_comp_instance(comp_data, entity_id.index);
	}
}

void
bent_remove(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return; }

	bent_index_t comp_index = reg.id - 1;
	if (!bent_bitset_check(&entity_data->components, comp_index)) { return; }

	bent_bitset_t old_components = entity_data->components;
	bent_bitset_t new_components = old_components;
	bent_bitset_unset(&new_components, comp_index);
	bent_notify_systems(world, entity_id, &old_components, &new_components);

	bent_component_data_t* comp_data = &world->components[comp_index];
	void* instance = bent_comp_instance(comp_data, entity_id.index);
	if (comp_data->def->cleanup) {
		comp_data->def->cleanup(instance);
	}
	bent_bitset_unset(&entity_data->components, comp_index);
}

void*
bent_get(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg) {
	const bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return NULL; }

	bent_index_t comp_index = reg.id - 1;
	if (!bent_bitset_check(&entity_data->components, comp_index)) { return NULL; }

	bent_component_data_t* comp_data = &world->components[comp_index];
	return bent_comp_instance(comp_data, entity_id.index);
}

bool
bent_has(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg) {
	const bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return false; }

	bent_index_t comp_index = reg.id - 1;
	return bent_bitset_check(&entity_data->components, comp_index);
}

void*
bent_get_sys_data(bent_world_t* world, bent_sys_reg_t sys) {
	return world->systems[sys.id - 1].userdata;
}

const char*
bent_get_sys_name(bent_world_t* world, bent_sys_reg_t sys) {
	return world->systems[sys.id - 1].name;
}

bent_t*
bent_get_entity_list(bent_world_t* world, bent_sys_reg_t sys, bent_index_t* num_entities) {
	bent_system_data_t* sys_data = &world->systems[sys.id - 1];

	if (num_entities) { *num_entities = (bent_index_t)barray_len(sys_data->dense); }

	return sys_data->dense;
}

bent_bitset_t
bent_get_entity_mask(bent_world_t* world, bent_t entity) {
	const bent_entity_data_t* entity_data = bent_entity_data(world, entity);
	return entity_data != NULL ? entity_data->components : (bent_bitset_t){ 0 };
}

void
bent_run(bent_world_t* world, bent_mask_t update_mask) {
	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t sys_index = 0; sys_index < num_systems; ++sys_index) {
		bent_system_data_t* sys = &world->systems[sys_index];
		if (sys->def->update && (sys->def->update_mask & update_mask) > 0) {
			world->defer_destruction = true;
			sys->def->update(
				sys->userdata,
				world,
				update_mask,
				sys->dense,
				(bent_index_t)barray_len(sys->dense)
			);
			world->defer_destruction = false;

			bent_drain_destroy_queue(world);
		}
	}
}

bool
bent_match(bent_world_t* world, bent_sys_reg_t reg, bent_t entity_id) {
	const bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return false; }

	const bent_system_data_t* sys = &world->systems[reg.id - 1];
	return bent_sys_match_impl(sys, &entity_data->components);
}

bent_index_t
bent__entity_list_len(bent_t* entities) {
	return (bent_index_t)barray_len(entities);
}

// serialization support {{{

void
bent_clear(bent_world_t* world) {
	BENT_ASSERT(!world->defer_destruction);

	BHANDLE_FOREACH(handle, &world->handles) {
		bent_destroy_immediately(world, bent__from_bhandle(handle));
	}
	barray_clear(world->destroy_queue);
}

bent_handles_t
bent_handles(bent_world_t* world) {
	bhandle_state_t state = bhandle_save(&world->handles);
	return (bent_handles_t){ .len = state.len, .gens = state.gens };
}

bent_index_t*
bent_load_handles_begin(bent_world_t* world, bent_index_t len) {
	BENT_ASSERT(bhandle_count(&world->handles) == 0);
	return bhandle_load_begin(&world->handles, len, world->memctx);
}

void
bent_load_handles_end(bent_world_t* world) {
	bhandle_load_end(&world->handles);

	// Every slot starts over, alive ones as empty entities
	bseg_clear(world->entities);
	bseg_resize(world->entities, bhandle_capacity(&world->handles), world->memctx);

	BHANDLE_FOREACH(handle, &world->handles) {
		bent_match_empty(world, bent__from_bhandle(handle));
	}
}

bool
bent_load_handles(bent_world_t* world, bent_handles_t handles) {
	bent_index_t* gens = bent_load_handles_begin(world, handles.len);
	if (gens == NULL && handles.len > 0) { return false; }

	if (handles.len > 0) {
		memcpy(gens, handles.gens, (size_t)handles.len * sizeof(bent_index_t));
	}
	bent_load_handles_end(world);
	return true;
}

bool
bent_reserve(bent_world_t* world, bent_t entity_id) {
	bhandle_t handle = bent__to_bhandle(entity_id);
	if (bhandle_is_valid(&world->handles, handle)) { return true; }
	if (bhandle_reserve(&world->handles, handle, world->memctx) < 0) { return false; }

	bent_reset_entity_data(world, handle.index);
	bent_match_empty(world, entity_id);
	return true;
}

void
bent_begin_load(bent_world_t* world) {
	BENT_ASSERT(!world->loading);
	BENT_ASSERT(!world->defer_destruction);
	BENT_ASSERT(bhandle_count(&world->handles) == 0);
	world->loading = true;
}

void
bent_end_load(bent_world_t* world) {
	BENT_ASSERT(world->loading);

	world->loading = false;

	BHANDLE_FOREACH(handle, &world->handles) {
		const bent_entity_data_t* entity_data = bseg_ref(world->entities, handle.index);
		bent_t entity_id = bent__from_bhandle(handle);

		// Replay creation and then the addition of every loaded component as
		// one step. Whatever the callbacks add on top is queued and applied
		// once the membership for the loaded set is established.
		bent_bitset_t empty = { 0 };
		bent_bitset_t loaded = entity_data->components;
		bool outermost = bent_begin_notify(world);
		BENT_ASSERT(outermost);
		bent_match_empty_impl(world, entity_id);
		bent_notify_systems_impl(world, entity_id, &empty, &loaded);
		bent_end_notify(world);
	}
}

void*
bent_restore(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg) {
	BENT_ASSERT(world->loading);

	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return NULL; }

	bent_index_t comp_index = reg.id - 1;
	if (bent_bitset_check(&entity_data->components, comp_index)) { return NULL; }

	bent_component_data_t* comp_data = &world->components[comp_index];
	void* instance = bent_comp_ensure_instance(comp_data, entity_id.index, world->memctx);
	if (instance != NULL) {
		memset(instance, 0, comp_data->def->size);
	}
	bent_bitset_set(&entity_data->components, comp_index);

	return instance;
}

bent_index_t
bent_count_with(bent_world_t* world, bent_comp_reg_t reg) {
	bent_index_t comp_index = reg.id - 1;
	bent_index_t count = 0;
	BHANDLE_FOREACH(handle, &world->handles) {
		const bent_entity_data_t* entity_data = bseg_ref(world->entities, handle.index);
		if (bent_bitset_check(&entity_data->components, comp_index)) { ++count; }
	}
	return count;
}

bent_comp_reg_t
bent_find_comp(const char* name) {
	AUTOLIST_FOREACH(itr, bent__components) {
		if (strcmp(itr->name, name) == 0) {
			return *(const bent_comp_reg_t*)itr->value_addr;
		}
	}
	return (bent_comp_reg_t){ 0 };
}

bent_sys_reg_t
bent_find_sys(const char* name) {
	AUTOLIST_FOREACH(itr, bent__systems) {
		if (strcmp(itr->name, name) == 0) {
			return *(const bent_sys_reg_t*)itr->value_addr;
		}
	}
	return (bent_sys_reg_t){ 0 };
}

const char*
bent_unserializable_comp(void) {
	BENT_FOREACH_COMP(itr) {
		if (bent_comp_save_mode(itr.comp.def) == BENT_COMP_SAVE_INVALID) {
			return itr.name;
		}
	}
	return NULL;
}

bent_t
bent__next_live(bent_world_t* world, bent_index_t from) {
	return bent__from_bhandle(bhandle__next_live(&world->handles, from));
}

bent_t
bent__next_with(bent_world_t* world, bent_comp_reg_t reg, bent_index_t from) {
	bent_index_t comp_index = reg.id - 1;
	for (
		bhandle_t handle = bhandle__next_live(&world->handles, from);
		!bhandle_is_null(handle);
		handle = bhandle__next_live(&world->handles, handle.index + 1)
	) {
		const bent_entity_data_t* entity_data = bseg_ref(world->entities, handle.index);
		if (bent_bitset_check(&entity_data->components, comp_index)) {
			return bent__from_bhandle(handle);
		}
	}
	return (bent_t){ 0 };
}

// }}}

#endif
