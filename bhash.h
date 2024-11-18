#ifndef BHASH_H
#define BHASH_H

/**
 * @file
 * @brief Type-safe hashtable
 *
 * Based on: https://nullprogram.com/blog/2022/08/08/
 *
 * Several functions in this library return an index.
 * If it is -1, it indicates a "not found" result.
 * Otherwise, it is an index into the @ref bhash_sample_t.keys and ref bhash_sample_t.values arrays.
 */

#ifndef BHASH_INDEX_TYPE
#include <stdint.h>
#define BHASH_INDEX_TYPE int32_t
#define BHASH_HASH_TYPE uint64_t
#endif

#ifndef BHASH_API
#define BHASH_API
#endif

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

typedef BHASH_INDEX_TYPE bhash_index_t;
typedef BHASH_HASH_TYPE bhash_hash_t;
typedef bhash_hash_t (*bhash_hash_fn_t)(const void* key, size_t size);
typedef bool (*bhash_eq_fn_t)(const void* lhs, const void* rhs, size_t size);

/*! Configuration for a hash table */
typedef struct bhash_config_s {
	/*! Hash function */
	bhash_hash_fn_t hash;

	/*! @brief Comparison function */
	bhash_eq_fn_t eq;

	/**
	 * @brief Load percentage of the hashtable.
	 *
	 * Must be the range [0, 100).
	 *
	 * When the table has more than this percentage of data, a rehash will be triggered.
	 */
	bhash_index_t load_percent;

	/**
	 * @brief Percentage of tombstones in the hashtable
	 *
	 * Must be the range [0, 100).
	 *
	 * When the table has more than this percentage of tombstone, a rehash will be triggered.
	 */
	bhash_index_t tombstone_percent;

	/*! Initial exponential */
	bhash_index_t initial_exp;

	/*! Whether the hashtable supports removal */
	bool removable;

	/*! Context passed to allocator */
	void* memctx;
} bhash_config_t;

/**
 * @brief The hashtable implementation details.
 *
 * Should be treated as opaque.
 */
typedef struct bhash_base_s {
	void* memctx;
	bhash_hash_fn_t hash;
	bhash_eq_fn_t eq;
	bhash_index_t load_percent;
	bhash_index_t tombstone_percent;
	size_t key_size;
	size_t value_size;
	bhash_index_t* indices;
	bhash_index_t* r_indices;
	bhash_hash_t* hashes;
	bhash_index_t len;
	bhash_index_t free_space;
	bhash_index_t exp;
} bhash_base_t;

/*! Result of @ref bhash_alloc */
typedef struct {
	/**
	 * @brief The index that is allocated for this entry
	 *
	 * This can be used to index into @ref bhash_sample_t.keys and @ref bhash_sample_t.values.
	 */
	bhash_index_t index;

	/*! Whether the entry is new */
	bool is_new;
} bhash_alloc_result_t;

/**
 * @brief Helper macro to define a hashtable type
 * @param K key type
 * @param V value type
 * @see bhash_sample_t
 */
#define BHASH_TABLE(K, V) \
	struct { \
		bhash_base_t base; \
		K* keys; \
		V* values; \
	}

/**
 * @brief Helper macro to define a hashset type
 * @param K key type
 * @see bhash_sample_t
 */
#define BHASH_SET(K) \
	struct { \
		bhash_base_t base; \
		K* keys; \
	}

#ifdef DOXYGEN

/*! A sample hashtable */
typedef struct {
	bhash_base_t base;
	/*! All keys of the table as a contiguouos array */
	K* keys;
	/*! All values of the table as a contiguouos array */
	V* values;
} bhash_sample_t;

#endif

/**
 * @brief Initialize a hashtable.
 *
 * @param table Address of the hashtable (defined with @ref BHASH_TABLE).
 * @param config The configuration (@ref bhash_config_t).
 *
 * @see bhash_sample_t
 */
#define bhash_init(table, config) \
	bhash__do_init( \
		&((table)->base), \
		sizeof((table)->keys[0]), \
		sizeof((table)->values[0]), \
		config \
	)

/**
 * @brief Initialize a hashtable.
 *
 * Same as a hashtable but without values.
 *
 * @param table Address of the hashtable (defined with @ref BHASH_SET).
 * @param config The configuration (@ref bhash_config_t).
 *
 * @see bhash_sample_t
 */
#define bhash_init_set(table, config) \
	bhash__do_init( \
		&((table)->base), \
		sizeof((table)->keys[0]), \
		0, \
		config \
	)

/*! Clear a table */
#define bhash_clear(table) bhash__do_clear(&((table)->base))

/**
 * @brief Free up all resources used by a hashtable.
 *
 * @see bhash_init
 */
#define bhash_cleanup(table) bhash__do_cleanup(&((table)->base))

/**
 * @brief Allocate an entry in the hashtable.
 *
 * @return @ref bhash_alloc_result_t
 */
#define bhash_alloc(table, key) \
	(BHASH__TYPECHECK_EXP((table)->keys[0], key), bhash__do_alloc(&((table)->base), &(key)))

/*! Add a new entry to the table */
#define bhash_put(table, key, value) \
	do { \
		bhash_index_t bhash__put_index = bhash_alloc(table, key).index; \
		BHASH__TYPECHECK_STMT((table)->keys[0], key); \
		BHASH__TYPECHECK_STMT((table)->values[0], value); \
		(table)->keys[bhash__put_index] = key; \
		(table)->values[bhash__put_index] = value; \
	} while (0)

/*! Add a new key to the table */
#define bhash_put_key(table, key) \
	do { \
		bhash_index_t bhash__put_index = bhash_alloc(table, key).index; \
		BHASH__TYPECHECK_STMT((table)->keys[0], key); \
		(table)->keys[bhash__put_index] = key; \
	} while (0)

/**
 * @brief Find an entry given its key.
 *
 * @return Index of the entry.
 */
#define bhash_find(table, key) \
	(BHASH__TYPECHECK_EXP((table)->keys[0], key), bhash__do_find(&((table)->base), &(key)))

/**
 * @brief Remove an entry.
 *
 * @return Index of the entry.
 * @remarks
 *   If the returned index is positive, the entry is found.
 *   Its data is still accessible until overwritten.
 *   User code can refer to it to free up other resources.
 */
#define bhash_remove(table, key) \
	(BHASH__TYPECHECK_EXP((table)->keys[0], key), bhash__do_remove(&((table)->base), &(key)))

/*! Check whether an index is valid */
#define bhash_is_valid(index) ((index) >= 0)

/*! Retrieve the length of the table */
#define bhash_len(table) ((table)->base.len)

/*! Validate and assert when the table violates some invariants */
#define bhash_validate(table) bhash__do_validate(&((table)->base))

// small, fast 64 bit hash function.
//
// https://github.com/N-R-K/ChibiHash
// https://nrk.neocities.org/articles/chibihash
//
// This is free and unencumbered software released into the public domain.
// For more information, please refer to <https://unlicense.org/>
static inline uint64_t
bhash__chibihash64__load64le(const uint8_t *p) {
	return (uint64_t)p[0] <<  0 | (uint64_t)p[1] <<  8 |
	       (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24 |
	       (uint64_t)p[4] << 32 | (uint64_t)p[5] << 40 |
	       (uint64_t)p[6] << 48 | (uint64_t)p[7] << 56;
}

static inline uint64_t
bhash__chibihash64(const void *keyIn, ptrdiff_t len, uint64_t seed) {
	const uint8_t *k = (const uint8_t *)keyIn;
	ptrdiff_t l = len;

	const uint64_t P1 = UINT64_C(0x2B7E151628AED2A5);
	const uint64_t P2 = UINT64_C(0x9E3793492EEDC3F7);
	const uint64_t P3 = UINT64_C(0x3243F6A8885A308C);

	uint64_t h[4] = { P1, P2, P3, seed };

	for (; l >= 32; l -= 32) {
		for (int i = 0; i < 4; ++i, k += 8) {
			uint64_t lane = bhash__chibihash64__load64le(k);
			h[i] ^= lane;
			h[i] *= P1;
			h[(i+1)&3] ^= ((lane << 40) | (lane >> 24));
		}
	}

	h[0] += ((uint64_t)len << 32) | ((uint64_t)len >> 32);
	if (l & 1) {
		h[0] ^= k[0];
		--l, ++k;
	}
	h[0] *= P2; h[0] ^= h[0] >> 31;

	for (int i = 1; l >= 8; l -= 8, k += 8, ++i) {
		h[i] ^= bhash__chibihash64__load64le(k);
		h[i] *= P2; h[i] ^= h[i] >> 31;
	}

	for (int i = 0; l > 0; l -= 2, k += 2, ++i) {
		h[i] ^= (k[0] | ((uint64_t)k[1] << 8));
		h[i] *= P3; h[i] ^= h[i] >> 31;
	}

	uint64_t x = seed;
	x ^= h[0] * ((h[2] >> 32)|1);
	x ^= h[1] * ((h[3] >> 32)|1);
	x ^= h[2] * ((h[0] >> 32)|1);
	x ^= h[3] * ((h[1] >> 32)|1);

	// moremur: https://mostlymangling.blogspot.com/2019/12/stronger-better-morer-moremur-better.html
	x ^= x >> 27; x *= UINT64_C(0x3C79AC492BA7B653);
	x ^= x >> 33; x *= UINT64_C(0x1C69B3F74AC4AE35);
	x ^= x >> 27;

	return x;
}

static inline bhash_hash_t
bhash_hash(const void* key, size_t size) {
	return (bhash_hash_t)bhash__chibihash64(key, size, 0);
}

static inline bool
bhash_eq(const void* lhs, const void* rhs, size_t size) {
	return memcmp(lhs, rhs, size) == 0;
}

/*! Default configuration */
static inline bhash_config_t
bhash_config_default(void) {
	return (bhash_config_t){
		.hash = bhash_hash,
		.eq = bhash_eq,
		.load_percent = 50,
		.tombstone_percent = 75,
		.initial_exp = 3,
		.removable = true,
	};
}

// Private

#ifndef DOXYGEN

#if __STDC_VERSION__ >= 202311L
#	define BHASH__TYPEOF(EXP) typeof(EXP)
#elif defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define BHASH__TYPEOF(EXP) __typeof__(EXP)
#endif

#ifdef BHASH__TYPEOF
// When typeof is available we can make a direct type comparison.
#	define BHASH__TYPECHECK_STMT(LHS, RHS) \
	_Static_assert( \
		_Generic(LHS, BHASH__TYPEOF(RHS): 1, default: 0), \
		"Type mismatch: `" #LHS "` and `" #RHS "` have different types" \
	)
// _Static_assert is actually quite hard to use in an expression.
// Jamming it into a struct: `sizeof(struct{_Static_assert(...);})` doesn't work
// quite well.
// Clang requires the RHS of BHASH__TYPECHECK_EXP to be a constant expression
// which is not always the case in typical user code.
// The good old negative size array works in all compilers but the error message
// is somewhat cryptic.
#	define BHASH__TYPECHECK_EXP(LHS, RHS) \
	(void)sizeof(char[_Generic(RHS, BHASH__TYPEOF(LHS): 1, default: -1)]) /* If you get an error here, you have the wrong type */
#else
// When it isn't we have to rely on size and assignability.
// Only check for size as this macro is only used when assignment is already made.
#	define BHASH__TYPECHECK_STMT(LHS, RHS) \
	_Static_assert( \
		sizeof(LHS) == sizeof(RHS), \
		"Type mismatch: `" #LHS "` and `" #RHS "` have different types" \
	)
// Check both size and assignability as integer types of different sizes are
// assignable (with warnings).
// We can't count on user having warnings enabled.
#	define BHASH__TYPECHECK_EXP(LHS, RHS) \
	((void)sizeof(LHS = RHS), (void)sizeof(char[sizeof(LHS) == sizeof(RHS) ? 1 : -1]))  /* If you get an error here, you have the wrong type */
#endif

BHASH_API void
bhash__do_init(bhash_base_t* bhash, size_t key_size, size_t value_size, bhash_config_t config);

BHASH_API bhash_alloc_result_t
bhash__do_alloc(bhash_base_t* bhash, const void* key);

BHASH_API bhash_index_t
bhash__do_find(bhash_base_t* bhash, const void* key);

BHASH_API bhash_index_t
bhash__do_remove(bhash_base_t* bhash, const void* key);

BHASH_API void
bhash__do_validate(bhash_base_t* bhash);

BHASH_API void
bhash__do_cleanup(bhash_base_t* bhash);

BHASH_API void
bhash__do_clear(bhash_base_t* bhash);

#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BHASH_IMPLEMENTATION)
#define BHASH_IMPLEMENTATION
#endif

#ifdef BHASH_IMPLEMENTATION

#define BHASH_EMPTY ((bhash_index_t)0)
#define BHASH_TOMBSTONE ((bhash_index_t)-1)

#ifndef BHASH_REALLOC
#	ifdef BLIB_REALLOC
#		define BHASH_REALLOC BLIB_REALLOC
#	else
#		define BHASH_REALLOC(ptr, size, ctx) bhash__libc_realloc(ptr, size, ctx)
#		define BHASH_USE_LIBC_REALLOC
#	endif
#endif

#ifdef BHASH_USE_LIBC_REALLOC
#include <stdlib.h>

static inline void*
bhash__libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

#ifndef BHASH_ASSERT
#include <stdio.h>
#include <stdlib.h>

#define BHASH_ASSERT(COND, MSG, ...) \
	if (!(COND)) { \
		fprintf(stderr, __FILE__ "(" BHASH_STRINGIFY(__LINE__) "): " MSG "\n", #COND, __VA_ARGS__); \
		abort(); \
	}
#define BHASH_STRINGIFY(X) BHASH_STRINGIFY2(X)
#define BHASH_STRINGIFY2(X) #X

#endif

typedef BHASH_TABLE(char, char) bhash_dummy_t;

static inline void**
bhash_keys_ptr(bhash_base_t* bhash) {
	return (void**)((char*)bhash + offsetof(bhash_dummy_t, keys) - offsetof(bhash_dummy_t, base));
}

static inline void**
bhash_values_ptr(bhash_base_t* bhash) {
	return (void**)((char*)bhash + offsetof(bhash_dummy_t, values) - offsetof(bhash_dummy_t, base));
}

static inline void*
bhash_key_at(bhash_base_t* bhash, bhash_index_t index) {
	return *(char**)bhash_keys_ptr(bhash) + index * bhash->key_size;
}

static inline void*
bhash_value_at(bhash_base_t* bhash, bhash_index_t index) {
	return *(char**)bhash_values_ptr(bhash) + index * bhash->value_size;
}

// https://nullprogram.com/blog/2022/08/08/
static inline bhash_index_t
bhash_lookup_index(bhash_hash_t hash, bhash_index_t exp, bhash_index_t idx) {
	uint32_t mask = ((uint32_t)1 << exp) - 1;
	uint32_t step = (uint32_t)((hash >> (64 - exp)) | 1);
	return (idx + step) & mask;
}

static inline void
bhash_maybe_grow(bhash_base_t* bhash) {
	if (bhash->free_space > 0) { return; }

	bhash_index_t exp = bhash->exp;
	bhash_index_t hash_capacity = 1 << exp;
	bhash_index_t data_capacity = hash_capacity * bhash->load_percent / 100;
	bhash_index_t num_tombstones = data_capacity - bhash->len;
	// Grow if there are not too many tombstone. Otherwise, do in-place rehash
	if (num_tombstones < data_capacity * bhash->tombstone_percent / 100) {
		bhash->exp = exp += 1;
		hash_capacity = 1 << exp;
		data_capacity = hash_capacity * bhash->load_percent / 100;
		bhash->indices = BHASH_REALLOC(bhash->indices, sizeof(bhash_index_t) * hash_capacity, bhash->memctx);
		bhash->hashes = BHASH_REALLOC(bhash->hashes, sizeof(bhash_hash_t) * data_capacity, bhash->memctx);
		if (bhash->r_indices) {
			bhash->r_indices = BHASH_REALLOC(bhash->r_indices, sizeof(bhash_index_t) * data_capacity, bhash->memctx);
		}

		bhash_index_t extra_space = bhash->r_indices != NULL ? 1 : 0;
		*bhash_keys_ptr(bhash) = BHASH_REALLOC(*bhash_keys_ptr(bhash), bhash->key_size * (data_capacity + extra_space), bhash->memctx);
		if (bhash->value_size > 0) {
			*bhash_values_ptr(bhash) = BHASH_REALLOC(*bhash_values_ptr(bhash), bhash->value_size * (data_capacity + extra_space), bhash->memctx);
		}
	}

	bhash_index_t len = bhash->len;
	bhash_index_t* indices = bhash->indices;
	bhash_index_t* r_indices = bhash->r_indices;
	bhash_hash_t* hashes = bhash->hashes;
	memset(bhash->indices, 0, sizeof(bhash_index_t) * hash_capacity);
	for (bhash_index_t i = 0; i < len; ++i) {
		bhash_hash_t hash = hashes[i];
		for (bhash_index_t hash_index = (bhash_index_t)hash;;) {
			hash_index = bhash_lookup_index(hash, exp, hash_index);
			bhash_index_t data_index = indices[hash_index];
			if (data_index == BHASH_EMPTY) {
				indices[hash_index] = i + 1;
				if (r_indices != NULL) { r_indices[i] = hash_index; }
				break;
			}
		}
	}
	bhash->free_space = data_capacity - bhash->len;
}

static inline void
bhash_find_impl(
	bhash_base_t* bhash,
	bhash_index_t* out_data_index,
	bhash_index_t* out_hash_index,
	const void* key
) {
	bhash_hash_t hash = bhash->hash(key, bhash->key_size);
	bhash_index_t* indices = bhash->indices;
	bhash_index_t exp = bhash->exp;
	bhash_hash_t* hashes = bhash->hashes;
	for (bhash_index_t hash_index = (bhash_index_t)hash;;) {
		hash_index = bhash_lookup_index(hash, exp, hash_index);
		bhash_index_t data_index = indices[hash_index];
		if (data_index == BHASH_EMPTY) {
			*out_data_index = *out_hash_index = -1;
			return;
		} else if (data_index == BHASH_TOMBSTONE) {
			continue;
		} else if (
			hashes[data_index - 1] == hash
			&& bhash->eq(key, bhash_key_at(bhash, data_index - 1), bhash->key_size)
		) {
			*out_data_index = data_index - 1;
			*out_hash_index = hash_index;
			return;
		}
	}
}

void
bhash__do_init(
	bhash_base_t* bhash,
	size_t key_size,
	size_t value_size,
	bhash_config_t config
) {
	bhash_index_t hash_capacity = 1 << config.initial_exp;
	bhash_index_t data_capacity = hash_capacity * config.load_percent / 100;
	bhash_index_t extra_space = config.removable ? 1 : 0; // Extra temp space for swapping
	(*bhash) = (bhash_base_t){
		.memctx = config.memctx,
		.hash = config.hash,
		.eq = config.eq,
		.load_percent = config.load_percent,
		.tombstone_percent = config.tombstone_percent,
		.key_size = key_size,
		.value_size = value_size,
		.indices = BHASH_REALLOC(NULL, sizeof(bhash_index_t) * hash_capacity, config.memctx),
		.hashes = BHASH_REALLOC(NULL, sizeof(bhash_hash_t) * data_capacity, config.memctx),
		.len = 0,
		.exp = config.initial_exp,
		.free_space = data_capacity,
	};
	memset(bhash->indices, 0, sizeof(bhash_index_t) * hash_capacity);

	if (config.removable) {
		bhash->r_indices = BHASH_REALLOC(NULL, sizeof(bhash_index_t) * data_capacity, config.memctx);
	}

	*bhash_keys_ptr(bhash) = BHASH_REALLOC(NULL, key_size * (data_capacity + extra_space), config.memctx);
	if (value_size > 0) {
		*bhash_values_ptr(bhash) = BHASH_REALLOC(NULL, value_size * (data_capacity + extra_space), config.memctx);
	} else {
		*bhash_values_ptr(bhash) = NULL;
	}
}

bhash_alloc_result_t
bhash__do_alloc(bhash_base_t* bhash, const void* key) {
	bhash_maybe_grow(bhash);
	bhash_hash_t hash = bhash->hash(key, bhash->key_size);
	bhash_index_t dest_slot = -1;
	bhash_index_t exp = bhash->exp;
	bhash_index_t* indices = bhash->indices;
	bhash_hash_t* hashes = bhash->hashes;
	for (bhash_index_t hash_index = (bhash_index_t)hash;;) {
		hash_index = bhash_lookup_index(hash, exp, hash_index);
		bhash_index_t data_index = indices[hash_index];
		if (data_index == BHASH_EMPTY) {
			bhash->free_space -= (dest_slot == -1); // New empty slot allocated
			dest_slot = dest_slot == -1 ? hash_index : dest_slot;
			data_index = bhash->len++;
			indices[dest_slot] = data_index + 1;
			if (bhash->r_indices) { bhash->r_indices[data_index] = dest_slot; }
			hashes[data_index] = hash;
			return (bhash_alloc_result_t){
				.index = data_index,
				.is_new = true,
			};
		} else if (data_index == BHASH_TOMBSTONE) {
			dest_slot = dest_slot == -1 ? hash_index : dest_slot;
		} else if (
			hashes[data_index - 1] == hash
			&& bhash->eq(key, bhash_key_at(bhash, data_index - 1), bhash->key_size)
		) {
			return (bhash_alloc_result_t){
				.index = data_index - 1,
				.is_new = false,
			};
		}
	}
}

bhash_index_t
bhash__do_find(bhash_base_t* bhash, const void* key) {
	bhash_index_t data_index;
	bhash_index_t hash_index;
	bhash_find_impl(bhash, &data_index, &hash_index, key);
	return data_index;
}

bhash_index_t
bhash__do_remove(bhash_base_t* bhash, const void* key) {
	if (bhash->r_indices == NULL) { return -1; }

	bhash_index_t remove_index, remove_r_index;
	bhash_index_t end_index = bhash->len;
	bhash_index_t tail_index = end_index - 1;
	bhash_find_impl(bhash, &remove_index, &remove_r_index, key);
	if (remove_index == -1) { return remove_index; }

	// Move the last element into the deleted slot and delete the last element
	bhash_index_t tail_r_index = bhash->r_indices[tail_index];
	bhash->indices[tail_r_index] = remove_index + 1;
	bhash->indices[remove_r_index] = BHASH_TOMBSTONE;
	bhash->r_indices[remove_index] = tail_r_index;
	bhash->hashes[remove_index] = bhash->hashes[tail_index];

	// Rotate key and values then point user code to the temp position at the end
	memcpy(bhash_key_at(bhash, end_index), bhash_key_at(bhash, remove_index), bhash->key_size);
	memcpy(bhash_key_at(bhash, remove_index), bhash_key_at(bhash, tail_index), bhash->key_size);
	if (bhash->value_size > 0) {
		memcpy(bhash_value_at(bhash, end_index), bhash_value_at(bhash, remove_index), bhash->value_size);
		memcpy(bhash_value_at(bhash, remove_index), bhash_value_at(bhash, tail_index), bhash->value_size);
	}

	bhash->len -= 1;
	return end_index;
}

void
bhash__do_validate(bhash_base_t* bhash) {
	bhash_index_t len = bhash->len;
	bhash_hash_t* hashes = bhash->hashes;
	bhash_index_t* indices = bhash->indices;
	bhash_index_t* r_indices = bhash->r_indices;
	for (bhash_index_t i = 0; i < len; ++i) {
		bhash_hash_t stored_hash = hashes[i];
		bhash_hash_t computed_hash = bhash->hash(bhash_key_at(bhash, i), bhash->key_size);
		BHASH_ASSERT(stored_hash == computed_hash, "%s: Hash mismatch at %d", i);
		bhash_index_t r_index = r_indices[i];
		bhash_index_t index = indices[r_index];
		BHASH_ASSERT(index == i + 1, "%s: Index mismatch at %d", i);
	}
	bhash_index_t hash_capacity = (bhash_index_t)1 << bhash->exp;
	bhash_index_t data_capacity = hash_capacity * bhash->load_percent / 100;
	BHASH_ASSERT(len <= data_capacity, "%s: Invalid length %d (max: %d)", len, data_capacity);
	for (bhash_index_t i = 0; i < hash_capacity; ++i) {
		bhash_index_t index = indices[i];
		if (index <= 0) {
			BHASH_ASSERT(
				index == BHASH_EMPTY || index == BHASH_TOMBSTONE,
				"%s: Invalid negative index %d",
				index
			);
		} else {
			BHASH_ASSERT(
				(index - 1) <= len,
				"%s: Invalid positive index %d",
				index
			);
			bhash_index_t r_index = r_indices[index - 1];
			BHASH_ASSERT(
				i == r_index,
				"%s: Index mismatch at %d",
				i
			);
		}
	}
}

void
bhash__do_cleanup(bhash_base_t* bhash) {
	BHASH_REALLOC(*bhash_keys_ptr(bhash), 0, bhash->memctx);
	if (bhash->value_size > 0) {
		BHASH_REALLOC(*bhash_values_ptr(bhash), 0, bhash->memctx);
	}
	BHASH_REALLOC(bhash->indices, 0, bhash->memctx);
	BHASH_REALLOC(bhash->r_indices, 0, bhash->memctx);
	BHASH_REALLOC(bhash->hashes, 0, bhash->memctx);
}

void
bhash__do_clear(bhash_base_t* bhash) {
	bhash->len = 0;
	bhash_index_t hash_capacity = 1 << bhash->exp;
	memset(bhash->indices, 0, sizeof(bhash_index_t) * hash_capacity);
	bhash->free_space = hash_capacity * bhash->load_percent / 100;
}

#endif
