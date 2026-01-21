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
 */

#include "autolist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

#ifndef BENT_LOG
#define BENT_LOG(...)
#endif

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
 * @param COM_TYPE type of the component's data
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

#define BENT_TAG_COMP(NAME) BENT_DEFINE_TAG_COMP(NAME)

#endif

/*! Handle to an entity world */
typedef struct bent_world_s bent_world_t;

/*! Index type used in the library */
typedef BENT_INDEX_TYPE bent_index_t;

/*! Bitmask type used in the library */
typedef BENT_MASK_TYPE bent_mask_t;

/**
 * Entity handle
 *
 * Each handle has a generation counter so using a stale handle belonging to
 * a destroyed entity is safe.
 *
 * An entity handle can also be zero-initialized.
 * In which case, it will be considered stale.
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
#ifndef DOXYGEN
	bent_index_t index;
	bent_index_t gen;
#endif
} bent_t;

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
	 * Whether this system's @ref bent_sys_def_t::init callback can be called
	 * on reload.
	 */
	bool allow_reinit;

	/**
	 * Optional initialization callback.
	 *
	 * This may be called multiple times if @ref bent_sys_def_t::allow_reinit is `true`.
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
	 * The @ref bent_sys_def_t::allow_reinit flag has no effect.
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
 * @param world_ptr pointer to a world
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
 * @remarks The returned pointer is temporary.
 *     Any further interaction with the same world may invalidate it.
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
 *     The returned pointer is temporary.
 *     Any further interaction with the world may invalidate it.
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
 * @param mask an update mask
 *
 * @see bent_sys_def_t::update_mask
 * @see bent_sys_def_t::update
 */
BENT_API void
bent_run(bent_world_t* world, bent_mask_t update_mask);

static inline bool
bent_equal(bent_t lhs, bent_t rhs) {
	return lhs.index == rhs.index && lhs.gen == rhs.gen;
}

// Private

#ifndef DOXYGEN

#define BENT__COMP_DEF_NAME(NAME) bent_comp_def_##NAME
#define BENT__SYS_DEF_NAME(NAME) bent_sys_def_##NAME

AUTOLIST_DECLARE(bent__components)
AUTOLIST_DECLARE(bent__systems)

BENT_API bent_index_t
bent__entity_list_len(bent_t* entities);

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
#define BENT_BITSET_LEN ((BENT_MAX_NUM_COMPONENT_TYPES + sizeof(bent_mask_t) * CHAR_BIT - 1) / (sizeof(bent_mask_t) * CHAR_BIT))

// We depend on barray but try to hide its symbols to avoid conflict.
// If it is compiled in the same unit, honor the existing symbol decision>

#ifndef BARRAY_IMPLEMENTATION
#define BARRAY_API static inline
#define BARRAY_IMPLEMENTATION
#include "barray.h"
#endif

#include <limits.h>
#include <string.h>

AUTOLIST_IMPL(bent__components)
AUTOLIST_IMPL(bent__systems)

typedef struct {
	bent_index_t length;
	uint8_t* data;
} bent_dyn_array_t;

typedef struct {
	bent_mask_t bits[BENT_BITSET_LEN];
} bent_bitset_t;

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
	bent_dyn_array_t instances;
	const bent_comp_def_t* def;
	char* name;
} bent_component_data_t;

typedef struct {
	bent_bitset_t components;
	bent_index_t generation: (sizeof(bent_index_t) * CHAR_BIT) - 2;
	bool destroyed: 1;
	bool destroy_later: 1;
} bent_entity_data_t;

struct bent_world_s {
	void* memctx;
	bool defer_destruction;

	barray(bent_system_data_t) systems;
	barray(bent_entity_data_t) entities;
	barray(bent_index_t) free_indices;
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

// dyn_array {{{

static void*
bent_dyn_array_at(
	bent_dyn_array_t* array,
	bent_index_t index,
	size_t elem_size
) {
	if (elem_size != 0) {
		return array->data + (index * elem_size);
	} else {
		return NULL;
	}
}

static void
bent_dyn_array_ensure_length(
	bent_dyn_array_t* array,
	bent_index_t length,
	size_t elem_size,
	void* memctx
) {
	if (array->length >= length || elem_size == 0) { return; }

	bent_index_t new_length = array->length * 2 > length ? array->length * 2 : length;
	array->data = BENT_REALLOC(array->data, elem_size * new_length, memctx);
	array->length = new_length;
}

static void
bent_dyn_array_cleanup(bent_dyn_array_t* array, void* memctx) {
	BENT_REALLOC(array->data, 0, memctx);
}

// }}}

// bitset {{{

static void
bent_bitset_clear(bent_bitset_t* bitset) {
	memset(bitset, 0, sizeof(*bitset));
}

static void
bent_bitset_set(bent_bitset_t* bitset, bent_index_t bit_index) {
	bent_index_t num_bits_per_mask = sizeof(bent_index_t) * CHAR_BIT;
	bent_index_t mask_index = bit_index / num_bits_per_mask;
	bent_mask_t mask = (bent_mask_t)1 << (bit_index % num_bits_per_mask);
	bitset->bits[mask_index] |= mask;
}

static void
bent_bitset_unset(bent_bitset_t* bitset, bent_index_t bit_index) {
	bent_index_t num_bits_per_mask = sizeof(bent_index_t) * CHAR_BIT;
	bent_index_t mask_index = bit_index / num_bits_per_mask;
	bent_mask_t mask = ~((bent_mask_t)1 << (bit_index % num_bits_per_mask));
	bitset->bits[mask_index] &= mask;
}

static void
bent_bitset_flip(bent_bitset_t* bitset) {
	for (bent_index_t i = 0; i < BENT_BITSET_LEN; ++i) {
		bitset->bits[i] = ~bitset->bits[i];
	}
}

static bool
bent_bitset_check(const bent_bitset_t* bitset, bent_index_t bit_index) {
	bent_index_t num_bits_per_mask = sizeof(bent_index_t) * CHAR_BIT;
	bent_index_t mask_index = bit_index / num_bits_per_mask;
	bent_mask_t mask = (bent_mask_t)1 << (bit_index % num_bits_per_mask);
	return (bitset->bits[mask_index] & mask) > 0;
}

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

// }}}

// component {{{

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
	bent_dyn_array_cleanup(&comp->instances, world->memctx);
	BENT_REALLOC(comp->name, 0, world->memctx);
}

// }}}

// system {{{

typedef bool (*bhash_eq_fn_t)(const void* lhs, const void* rhs, size_t size);

static bool
bent_sys_match_impl(const bent_system_data_t* sys, const bent_bitset_t* components) {
	return bent_bitset_all_match(components, &sys->require)  // Match all of the requirements
		&& !bent_bitset_any_match(components, &sys->exclude);  // Match none of the exclusions
}

static void
bent_sys_add_entity(bent_world_t* world, bent_system_data_t* sys, bent_t entity) {
	bent_index_t sparse_size = (bent_index_t)barray_len(sys->sparse);
	if (entity.index - 1 >= sparse_size) {
		bent_index_t new_sparse_size = sparse_size * 2 > entity.index ? sparse_size * 2 : entity.index;
		barray_resize(sys->sparse, new_sparse_size, world->memctx);
	}
	sys->sparse[entity.index - 1] = (bent_index_t)barray_len(sys->dense);
	barray_push(sys->dense, entity, world->memctx);

	if (sys->def->add) {
		sys->def->add(sys->userdata, world, entity);
	}
}

static void
bent_sys_remove_entity(bent_world_t* world, bent_system_data_t* sys, bent_t entity) {
	bent_index_t dense_index = sys->sparse[entity.index - 1];
	bent_t last_entity = barray_pop(sys->dense);
	sys->dense[dense_index] = last_entity;
	sys->sparse[last_entity.index - 1] = dense_index;

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
		for (
			bent_comp_reg_t** comp = def->require;
			comp != NULL && *comp != NULL;
			++comp
		) {
			bent_bitset_set(&sys->require, (*comp)->id - 1);
		}

		for (
			bent_comp_reg_t** comp = def->exclude;
			comp != NULL && *comp != NULL;
			++comp
		) {
			bent_bitset_set(&sys->exclude, (*comp)->id - 1);
		}
	}

	bool initialized = sys->initialized;
	if (def->size) {
		sys->userdata = BENT_REALLOC(sys->userdata, def->size, world->memctx);
		if (!initialized) {
			memset(sys->userdata, 0, def->size);
		}
	}

	if (def->init && (!initialized || def->allow_reinit)) {
		def->init(sys->userdata, world);
	}

#ifndef BENT_NO_RELOAD
	// Update component inclusion based on new filter
	// Also, allow newly created systems to immediately register existing entities
	bent_system_data_t old_sys = {
		.require = old_require,
		.exclude = old_exclude,
	};
	bent_index_t num_entities = (bent_index_t)barray_len(world->entities);
	for (bent_index_t i = 0; i < num_entities; ++i) {
		const bent_entity_data_t* entity = &world->entities[i];
		if (world->entities[i].destroyed) { continue; }

		const bent_bitset_t* components = &entity->components;
		if (sys->initialized) {  // Existing system, do diff
			if (bent_sys_match_impl(&old_sys, components)) {
				if (!bent_sys_match_impl(sys, components)) {
					bent_sys_remove_entity(world, sys, (bent_t){
						.index = i + 1,
						.gen = entity->generation,
					});
				}
			} else {
				if (bent_sys_match_impl(sys, components)) {
					bent_sys_add_entity(world, sys, (bent_t){
						.index = i + 1,
						.gen = entity->generation,
					});
				}
			}
		} else {  // Newly registered system, try to match existing entities
			if (bent_sys_match_impl(sys, components)) {
				bent_sys_add_entity(world, sys, (bent_t){
					.index = i + 1,
					.gen = entity->generation,
				});
			}
		}
	}
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
bent_notify_systems(
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

// }}}

static void
bent_destroy_immediately(bent_world_t* world, bent_t entity_id) {
	bent_entity_data_t* entity_data = &world->entities[entity_id.index - 1];
	entity_data->destroyed = true;
	++entity_data->generation;

	const bent_bitset_t* components = &entity_data->components;

	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t i = 0; i < num_systems; ++i) {
		bent_system_data_t* sys = &world->systems[i];
		if (bent_sys_match_impl(sys, components)) {
			bent_sys_remove_entity(world, sys, entity_id);
		}
	}

	bent_index_t num_components = world->num_components;
	for (bent_index_t i = 0; i < num_components; ++i) {
		bent_component_data_t* comp = &world->components[i];
		if (
			bent_bitset_check(components, i)
			&&
			comp->def->cleanup
		) {
			comp->def->cleanup(bent_dyn_array_at(
				&comp->instances,
				entity_id.index - 1,
				comp->def->size
			));
		}
	}

	barray_push(world->free_indices, entity_id.index - 1, world->memctx);
}

static bent_entity_data_t*
bent_entity_data(bent_world_t* world, bent_t entity_id) {
	bent_index_t index = entity_id.index - 1;  // 0 wraps around
	if (index >= (bent_index_t)barray_len(world->entities)) { return NULL; }

	bent_entity_data_t* entity_data = &world->entities[index];
	if (entity_data->generation != entity_id.gen) { return NULL; }

	return entity_data;
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

	bent_index_t num_entities = (bent_index_t)barray_len(world->entities);
	for (bent_index_t i = 0; i < num_entities; ++i) {
		if (!world->entities[i].destroyed) {
			bent_destroy_immediately(world, (bent_t){
				.index = i + 1,
				.gen = world->entities[i].generation,
			});
		}
	}

	bent_index_t num_components = world->num_components;
	for (bent_index_t i = 0; i < num_components; ++i) {
		bent_comp_cleanup(world, &world->components[i]);
	}

	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t i = 0; i < num_systems; ++i) {
		bent_sys_cleanup(world, &world->systems[i]);
	}

	barray_free(world->systems, world->memctx);
	barray_free(world->entities, world->memctx);
	barray_free(world->free_indices, world->memctx);
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
	bent_index_t index;
	if (barray_len(world->free_indices) > 0) {
		index = barray_pop(world->free_indices);
		world->entities[index].destroyed = false;
		world->entities[index].destroy_later = false;
		bent_bitset_clear(&world->entities[index].components);
	} else {
		index = (bent_index_t)barray_len(world->entities);
		barray_push(world->entities, (bent_entity_data_t){ 0 }, world->memctx);
	}

	bent_t entity_id = {
		.index = index + 1,
		.gen = world->entities[index].generation,
	};

	// For completeness sake and also for consistent reload behavior, an empty
	// entity can match some systems if they have no requirement
	bent_bitset_t empty = { 0 };
	bent_index_t num_systems = (bent_index_t)barray_len(world->systems);
	for (bent_index_t i = 0; i < num_systems; ++i) {
		bent_system_data_t* sys = &world->systems[i];
		if (bent_sys_match_impl(sys, &empty)) {
			bent_sys_add_entity(world, sys, entity_id);
		}
	}

	return entity_id;
}

void
bent_destroy(bent_world_t* world, bent_t entity_id) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return; }

	if (world->defer_destruction) {
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
		size_t comp_size = comp_data->def->size;
		bent_dyn_array_ensure_length(
			&comp_data->instances,
			entity_id.index,
			comp_size,
			world->memctx
		);
		void* instance = bent_dyn_array_at(&comp_data->instances, entity_id.index - 1, comp_size);
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
		bent_notify_systems(world, entity_id, &old_components, &entity_data->components);

		return instance;
	} else {
		// Already added, return existing data
		return bent_dyn_array_at(&comp_data->instances, entity_id.index - 1, comp_data->def->size);
	}
}

void
bent_remove(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg) {
	bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return; }

	bent_index_t comp_index = reg.id - 1;
	if (!bent_bitset_check(&entity_data->components, comp_index)) { return; }

	bent_bitset_t old_components = entity_data->components;
	bent_bitset_unset(&entity_data->components, comp_index);
	bent_notify_systems(world, entity_id, &old_components, &entity_data->components);

	bent_component_data_t* comp_data = &world->components[comp_index];
	size_t comp_size = comp_data->def->size;
	void* instance = bent_dyn_array_at(&comp_data->instances, entity_id.index - 1, comp_size);
	if (comp_data->def->cleanup) {
		comp_data->def->cleanup(instance);
	}
}

void*
bent_get(bent_world_t* world, bent_t entity_id, bent_comp_reg_t reg) {
	const bent_entity_data_t* entity_data = bent_entity_data(world, entity_id);
	if (entity_data == NULL) { return NULL; }

	bent_index_t comp_index = reg.id - 1;
	if (!bent_bitset_check(&entity_data->components, comp_index)) { return NULL; }

	bent_component_data_t* comp_data = &world->components[comp_index];
	return bent_dyn_array_at(&comp_data->instances, entity_id.index - 1, comp_data->def->size);
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

			// Check queue length every iteration since destruction could lead
			// to more destruction
			for (bent_index_t queue_index = 0; queue_index < (bent_index_t)barray_len(world->destroy_queue); ++queue_index) {
				bent_destroy_immediately(world, world->destroy_queue[queue_index]);
			}
			barray_clear(world->destroy_queue);
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

#endif
