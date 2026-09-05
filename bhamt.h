// vim: set foldmethod=marker foldlevel=0:
#ifndef BHAMT_H
#define BHAMT_H

/**
 * @file
 * @brief Type-safe hash trie
 *
 * Based on: https://nullprogram.com/blog/2023/09/30/
 *
 * This is a hash trie: a hash-ordered trie with
 * `2^BHAMT_NUM_BITS` children per node.
 * Compared to @ref bhash.h, it has several distinguishing properties:
 *
 * * A zero-initialized table is a valid empty table.
 *   Initialization with @ref bhamt_init is only needed for custom hash/eq
 *   functions.
 * * Entries are allocated one node at a time and never move or get freed.
 *   Pointers to keys and values are stable for the lifetime of the table.
 * * There is no resizing, no removal and no cleanup.
 *   It is designed for arena allocation: the entire table is freed by
 *   resetting the arena.
 * * The table handle is small (3 pointers).
 *   It is cheap to embed many (mostly empty) tables in other structures.
 *
 * Typical use cases include: symbol tables, scoped environments,
 * deduplication and interning.
 *
 * Iteration order is unspecified and depends on hash values.
 * For deterministic iteration (e.g: codegen), maintain a separate array of
 * entries in insertion order and use the trie only for lookup.
 *
 * The key passed to the macros in this file must be an lvalue as its address
 * will be taken.
 */

#ifndef BHAMT_API
#define BHAMT_API
#endif

/**
 * @brief Number of hash bits consumed per trie level.
 *
 * Each node has @ref BHAMT_NUM_CHILDREN children.
 * Higher values make the trie shallower (faster lookup) but each node larger.
 *
 * Like the allocator, this must be defined consistently across all files
 * including this header, along with the implementation.
 */
#ifndef BHAMT_NUM_BITS
#define BHAMT_NUM_BITS 2
#endif

#if BHAMT_NUM_BITS < 1 || BHAMT_NUM_BITS > 8
#error "BHAMT_NUM_BITS must be in the range [1, 8]"
#endif

/*! Number of children per node */
#define BHAMT_NUM_CHILDREN (1 << BHAMT_NUM_BITS)

/**
 * @brief Capacity of the traversal stack in @ref bhamt_iter_t.
 *
 * The default is enough for any table using a well-distributed 64-bit hash.
 */
#ifndef BHAMT_ITER_STACK_SIZE
#define BHAMT_ITER_STACK_SIZE \
	((BHAMT_NUM_CHILDREN - 1) * (64 / BHAMT_NUM_BITS) + 64)
#endif

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint64_t bhamt_hash_t;
typedef bhamt_hash_t (*bhamt_hash_fn_t)(const void* key, size_t size);
typedef bool (*bhamt_eq_fn_t)(const void* lhs, const void* rhs, size_t size);

/**
 * @brief The table implementation details.
 *
 * Should be treated as opaque.
 *
 * A zero-initialized base is valid and uses the default hash/eq functions.
 */
typedef struct bhamt_base_s {
	bhamt_hash_fn_t hash;
	bhamt_eq_fn_t eq;
} bhamt_base_t;

/**
 * @brief Memory layout of a node, derived from the table type at each call site.
 *
 * Should be treated as opaque.
 */
typedef struct bhamt_spec_s {
	size_t node_size;
	size_t node_align;
	size_t key_size;
	size_t key_offset;
} bhamt_spec_t;

/*! Iterator state for @ref BHAMT_FOREACH */
typedef struct bhamt_iter_s {
	void* stack[BHAMT_ITER_STACK_SIZE];
	int32_t len;
} bhamt_iter_t;

/**
 * @brief Helper macro to define a table type
 * @param K key type
 * @param V value type
 * @see bhamt_sample_t
 */
#define BHAMT_TABLE(K, V) \
	struct { \
		bhamt_base_t base; \
		struct { \
			void* bhamt__children[BHAMT_NUM_CHILDREN]; \
			K key; \
			V value; \
		}* root; \
	}

/**
 * @brief Helper macro to define a set type
 * @param K key type
 * @see bhamt_sample_t
 */
#define BHAMT_SET(K) \
	struct { \
		bhamt_base_t base; \
		struct { \
			void* bhamt__children[BHAMT_NUM_CHILDREN]; \
			K key; \
		}* root; \
	}

#ifdef DOXYGEN

/*! A sample table */
typedef struct {
	bhamt_base_t base;
	/**
	 * @brief The root node of the trie.
	 *
	 * NULL when the table is empty.
	 * Each node has a `key` and (for tables) a `value` member which may be
	 * read freely.
	 * The value may also be modified in-place.
	 * The key must not be modified as that would corrupt the trie.
	 */
	bhamt_node_t* root;
} bhamt_sample_t;

#endif

/**
 * @brief Initialize a table with custom functions.
 *
 * This is optional.
 * A zero-initialized table is valid and uses the default functions:
 * hashing and comparing the raw bytes of the key.
 *
 * Custom functions are needed when the key contains pointers to external
 * data (e.g: strings) or padding.
 *
 * @param table Address of the table (defined with @ref BHAMT_TABLE or @ref BHAMT_SET).
 * @param hash_fn Hash function (@ref bhamt_hash_fn_t). NULL to use the default.
 * @param eq_fn Comparison function (@ref bhamt_eq_fn_t). NULL to use the default.
 *
 * @see bhamt_sample_t
 */
#define bhamt_init(table, hash_fn, eq_fn) \
	do { \
		(table)->base.hash = (hash_fn); \
		(table)->base.eq = (eq_fn); \
		(table)->root = NULL; \
	} while (0)

/**
 * @brief Find a node given its key.
 *
 * @return Pointer to the node or NULL if not found.
 *   The node's `key` and `value` members may be accessed.
 */
#define bhamt_find(table, KEY) \
	(BHAMT__TYPECHECK_EXP((table)->root->key, KEY), \
	(BHAMT__TYPEOF((table)->root))bhamt__do_find( \
		&(table)->base, (table)->root, &(KEY), BHAMT__SPEC(table)))

/**
 * @brief Retrieve a pointer to a value given its key.
 *
 * @return Pointer to the value or NULL if not found.
 */
#define bhamt_get(table, KEY) \
	(BHAMT__TYPECHECK_EXP((table)->root->key, KEY), \
	(BHAMT__TYPEOF(&(table)->root->value))bhamt__value_at( \
		bhamt__do_find(&(table)->base, (table)->root, &(KEY), BHAMT__SPEC(table)), \
		BHAMT__MEMBER_OFFSET((table)->root, value)))

/*! Check whether a table contains a key */
#define bhamt_has(table, key) (bhamt_find(table, key) != NULL)

/*! Check whether a table is empty */
#define bhamt_is_empty(table) ((table)->root == NULL)

/**
 * @brief Ensure that an entry with the given key exists.
 *
 * If the entry does not exist, it is created by copying the given key.
 * The value is zero-initialized.
 *
 * Otherwise, the existing entry is retrieved.
 *
 * The returned pointer remains valid for the lifetime of the table.
 *
 * @param table Address of the table.
 * @param key The key. Must be an lvalue.
 * @param memctx Context passed to the allocator.
 * @return Pointer to the value of the entry.
 *
 * @see bhamt_upsert_ex
 */
#define bhamt_upsert(table, key, memctx) \
	bhamt_upsert_ex(table, key, memctx, NULL)

/**
 * @brief Same as @ref bhamt_upsert but also reports whether the entry is new.
 *
 * @param out_is_new Pointer to a `bool` set to whether a new entry was
 *   created. May be NULL.
 */
#define bhamt_upsert_ex(table, KEY, memctx, out_is_new) \
	(BHAMT__TYPECHECK_EXP((table)->root->key, KEY), \
	(BHAMT__TYPEOF(&(table)->root->value))bhamt__value_at( \
		bhamt__do_upsert( \
			&(table)->base, (void**)&(table)->root, &(KEY), BHAMT__SPEC(table), \
			(memctx), (out_is_new)), \
		BHAMT__MEMBER_OFFSET((table)->root, value)))

/*! Add a new entry to the table, overwriting any existing value */
#define bhamt_put(table, KEY, VALUE, memctx) \
	do { \
		BHAMT__TYPECHECK_STMT((table)->root->value, VALUE); \
		*bhamt_upsert(table, KEY, memctx) = (VALUE); \
	} while (0)

/**
 * @brief Add a key to a set (defined with @ref BHAMT_SET).
 *
 * @return Whether the key was newly added.
 *   `false` means it was already a member.
 */
#define bhamt_set_add(table, KEY, memctx) \
	(BHAMT__TYPECHECK_EXP((table)->root->key, KEY), \
	bhamt__do_set_add( \
		&(table)->base, (void**)&(table)->root, &(KEY), BHAMT__SPEC(table), \
		(memctx)))

/*! Validate and assert when the table violates some invariants */
#define bhamt_validate(table) \
	bhamt__do_validate(&(table)->base, (table)->root, BHAMT__SPEC(table))

/**
 * @brief For each helper
 *
 * NODE iterates over all nodes of the table.
 * Its `key` and `value` members may be accessed.
 *
 * Iteration order is unspecified.
 * The table must not be modified during iteration.
 *
 * @param NODE node variable
 * @param TABLE address of the table to iterate
 */
#define BHAMT_FOREACH(NODE, TABLE) \
	for ( \
		struct { bhamt_iter_t iter; char once; } bhamt__s = { \
			.iter = { \
				.stack = { (TABLE)->root }, \
				.len = (TABLE)->root != NULL ? 1 : 0, \
			}, \
			.once = 1, \
		}; \
		bhamt__s.once; \
		bhamt__s.once = 0 \
	) \
		for ( \
			BHAMT__TYPEOF((TABLE)->root) NODE = bhamt__iter_next(&bhamt__s.iter); \
			NODE != NULL; \
			NODE = bhamt__iter_next(&bhamt__s.iter) \
		)

/*! Default hash function: FNV-style hash over the raw bytes of the key */
static inline bhamt_hash_t
bhamt_hash(const void* key, size_t size) {
	const unsigned char* bytes = key;
	bhamt_hash_t h = 0x100;
	for (size_t i = 0; i < size; ++i) {
		h ^= bytes[i];
		h *= UINT64_C(1111111111111111111);
	}
	return h;
}

/*! Default comparison function: memcmp over the raw bytes of the key */
static inline bool
bhamt_eq(const void* lhs, const void* rhs, size_t size) {
	return memcmp(lhs, rhs, size) == 0;
}

// Private

#ifndef DOXYGEN

#if __STDC_VERSION__ >= 202311L
#	define BHAMT__TYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BHAMT__TYPEOF(EXP) __typeof__(EXP)
#endif

#if defined(__clang__) || defined(__GNUC__)
#	define BHAMT__ALIGNOF(EXP) __alignof__(EXP)
#elif defined(_MSC_VER)
#	define BHAMT__ALIGNOF(EXP) __alignof(EXP)
#else
#	define BHAMT__ALIGNOF(EXP) _Alignof(BHAMT__TYPEOF(EXP))
#endif

#ifdef BHAMT__TYPEOF
// When typeof is available we can make a direct type comparison.
#	define BHAMT__TYPECHECK_STMT(LHS, RHS) \
	_Static_assert( \
		_Generic(LHS, BHAMT__TYPEOF(RHS): 1, default: 0), \
		"Type mismatch: `" #LHS "` and `" #RHS "` have different types" \
	)
#	define BHAMT__TYPECHECK_EXP(LHS, RHS) \
	(void)sizeof(char[_Generic(RHS, BHAMT__TYPEOF(LHS): 1, default: -1)]) /* If you get an error here, you have the wrong type */
#else
// When it isn't we have to rely on size and assignability.
#	define BHAMT__TYPECHECK_STMT(LHS, RHS) \
	_Static_assert( \
		sizeof(LHS) == sizeof(RHS), \
		"Type mismatch: `" #LHS "` and `" #RHS "` have different types" \
	)
#	define BHAMT__TYPECHECK_EXP(LHS, RHS) \
	((void)sizeof(LHS = RHS), (void)sizeof(char[sizeof(LHS) == sizeof(RHS) ? 1 : -1]))  /* If you get an error here, you have the wrong type */
#endif

#if defined(__clang__) || defined(__GNUC__) || __STDC_VERSION__ >= 202311L
// offsetof accepts a typeof type so no object is needed
#	define BHAMT__MEMBER_OFFSET(PTR, MEMBER) \
	offsetof(BHAMT__TYPEOF(*(PTR)), MEMBER)
#else
// Classic offsetof through pointer arithmetic.
// This also works when PTR is NULL, in the same vein as BCONTAINER_OF.
#	define BHAMT__MEMBER_OFFSET(PTR, MEMBER) \
	((size_t)((char*)&(PTR)->MEMBER - (char*)(PTR)))
#endif

#define BHAMT__SPEC(TABLE) \
	(&(bhamt_spec_t){ \
		.node_size = sizeof(*(TABLE)->root), \
		.node_align = BHAMT__ALIGNOF(*(TABLE)->root), \
		.key_size = sizeof((TABLE)->root->key), \
		.key_offset = BHAMT__MEMBER_OFFSET((TABLE)->root, key), \
	})

static inline void*
bhamt__value_at(void* node, size_t value_offset) {
	return node != NULL ? (char*)node + value_offset : NULL;
}

BHAMT_API void*
bhamt__do_find(
	const bhamt_base_t* base,
	const void* root,
	const void* key,
	const bhamt_spec_t* spec
);

BHAMT_API void*
bhamt__do_upsert(
	const bhamt_base_t* base,
	void** root,
	const void* key,
	const bhamt_spec_t* spec,
	void* memctx,
	bool* out_is_new
);

BHAMT_API bool
bhamt__do_set_add(
	const bhamt_base_t* base,
	void** root,
	const void* key,
	const bhamt_spec_t* spec,
	void* memctx
);

BHAMT_API void*
bhamt__iter_next(bhamt_iter_t* iter);

BHAMT_API void
bhamt__do_validate(const bhamt_base_t* base, const void* root, const bhamt_spec_t* spec);

#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BHAMT_IMPLEMENTATION)
#define BHAMT_IMPLEMENTATION
#endif

#ifdef BHAMT_IMPLEMENTATION

#ifndef BHAMT_ALLOC
#	ifdef BLIB_REALLOC
#		define BHAMT_ALLOC(size, align, ctx) BLIB_REALLOC(NULL, size, ctx)
#	else
#		define BHAMT_ALLOC(size, align, ctx) bhamt__libc_alloc(size, align, ctx)
#		define BHAMT_USE_LIBC_ALLOC
#	endif
#endif

#ifdef BHAMT_USE_LIBC_ALLOC
#include <stdlib.h>

static inline void*
bhamt__libc_alloc(size_t size, size_t align, void* ctx) {
	(void)align;
	(void)ctx;
	return malloc(size);
}

#endif

#ifndef BHAMT_ASSERT
#include <stdio.h>
#include <stdlib.h>

#define BHAMT_ASSERT(COND, MSG, ...) \
	if (!(COND)) { \
		fprintf(stderr, __FILE__ "(" BHAMT_STRINGIFY(__LINE__) "): " MSG "\n", #COND, __VA_ARGS__); \
		abort(); \
	}
#define BHAMT_STRINGIFY(X) BHAMT_STRINGIFY2(X)
#define BHAMT_STRINGIFY2(X) #X

#endif

static inline bhamt_hash_fn_t
bhamt__hash_fn(const bhamt_base_t* base) {
	return base->hash != NULL ? base->hash : bhamt_hash;
}

static inline bhamt_eq_fn_t
bhamt__eq_fn(const bhamt_base_t* base) {
	return base->eq != NULL ? base->eq : bhamt_eq;
}

void*
bhamt__do_find(
	const bhamt_base_t* base,
	const void* root,
	const void* key,
	const bhamt_spec_t* spec
) {
	bhamt_hash_fn_t hash_fn = bhamt__hash_fn(base);
	bhamt_eq_fn_t eq_fn = bhamt__eq_fn(base);

	const void* node = root;
	for (bhamt_hash_t h = hash_fn(key, spec->key_size); node != NULL; h <<= BHAMT_NUM_BITS) {
		if (eq_fn(key, (const char*)node + spec->key_offset, spec->key_size)) {
			return (void*)node;
		}
		node = ((void* const*)node)[h >> (64 - BHAMT_NUM_BITS)];
	}
	return NULL;
}

void*
bhamt__do_upsert(
	const bhamt_base_t* base,
	void** root,
	const void* key,
	const bhamt_spec_t* spec,
	void* memctx,
	bool* out_is_new
) {
	bhamt_hash_fn_t hash_fn = bhamt__hash_fn(base);
	bhamt_eq_fn_t eq_fn = bhamt__eq_fn(base);

	void** slot = root;
	for (bhamt_hash_t h = hash_fn(key, spec->key_size); *slot != NULL; h <<= BHAMT_NUM_BITS) {
		void* node = *slot;
		if (eq_fn(key, (char*)node + spec->key_offset, spec->key_size)) {
			if (out_is_new != NULL) { *out_is_new = false; }
			return node;
		}
		slot = (void**)node + (h >> (64 - BHAMT_NUM_BITS));
	}

	void* new_node = BHAMT_ALLOC(spec->node_size, spec->node_align, memctx);
	memset(new_node, 0, spec->node_size);
	memcpy((char*)new_node + spec->key_offset, key, spec->key_size);
	*slot = new_node;
	if (out_is_new != NULL) { *out_is_new = true; }
	return new_node;
}

bool
bhamt__do_set_add(
	const bhamt_base_t* base,
	void** root,
	const void* key,
	const bhamt_spec_t* spec,
	void* memctx
) {
	bool is_new;
	bhamt__do_upsert(base, root, key, spec, memctx, &is_new);
	return is_new;
}

void*
bhamt__iter_next(bhamt_iter_t* iter) {
	if (iter->len == 0) { return NULL; }

	void* node = iter->stack[--iter->len];
	void* const* children = node;
	for (int i = 0; i < BHAMT_NUM_CHILDREN; ++i) {
		if (children[i] != NULL) {
			BHAMT_ASSERT(
				iter->len < BHAMT_ITER_STACK_SIZE,
				"%s: Iterator stack overflow (%d)",
				(int)BHAMT_ITER_STACK_SIZE
			);
			iter->stack[iter->len++] = children[i];
		}
	}
	return node;
}

void
bhamt__do_validate(const bhamt_base_t* base, const void* root, const bhamt_spec_t* spec) {
	bhamt_iter_t iter = { .len = 0 };
	if (root != NULL) { iter.stack[iter.len++] = (void*)root; }

	// Every node must be findable through its own key.
	// This verifies both its position in the trie and the uniqueness of
	// its key.
	const void* node;
	while ((node = bhamt__iter_next(&iter)) != NULL) {
		const void* found = bhamt__do_find(
			base, root, (const char*)node + spec->key_offset, spec
		);
		BHAMT_ASSERT(
			found == node,
			"%s: Node %p is not reachable through its key",
			node
		);
	}
}

#endif
