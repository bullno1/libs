#ifndef BHASH_H
#define BHASH_H

#ifndef BHASH_INDEX_TYPE
#include <stdint.h>
#define BHASH_INDEX_TYPE int32_t
#define BHASH_HASH_TYPE uint64_t
#endif

#ifndef BHASH_INITIAL_EXP
#define BHASH_INITIAL_EXP 3
#endif

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

typedef BHASH_INDEX_TYPE bhash_index_t;
typedef BHASH_HASH_TYPE bhash_hash_t;
typedef bhash_hash_t (*bhash_hash_fn_t)(const void* key, size_t size);
typedef bool (*bhash_eq_fn_t)(const void* lhs, const void* rhs, size_t size);

typedef struct bhash_config_s {
	bhash_hash_fn_t hash;
	bhash_eq_fn_t eq;
	bool removable;
	bool has_values;
	void* memctx;
} bhash_config_t;

typedef struct bhash_base_s {
	bhash_index_t* indices;
	bhash_index_t* r_indices;
	bhash_hash_t* hashes;
	bhash_index_t len;
	bhash_index_t exp;
} bhash_base_t;

#define BHASH_EMPTY ((bhash_index_t)0)
#define BHASH_TOMBSTONE ((bhash_index_t)-1)

#define BHASH_TABLE(K, V) \
	struct { \
		bhash_base_t base; \
		K* keys; \
		V* values; \
	}

#define bhash_clear(result, table, key, config) \
	if (table != NULL) { \
		memset(table->base.indicies, 0, sizeof(bhash_index_t) * (1 << table->base.exp)); \
		table->base.len = 0; \
	}

#define bhash_destroy(table, config) \
	if (table != NULL) { \
		BHASH_REALLOC(table->keys, 0, config.memctx); \
		BHASH_REALLOC(table->values, 0, config.memctx); \
		BHASH_REALLOC(table->base.indicies, 0, config.memctx); \
		BHASH_REALLOC(table->base.r_indicies, 0, config.memctx); \
		BHASH_REALLOC(table->base.hashes, 0, config.memctx); \
		BHASH_REALLOC(table, 0, config.memctx); \
		table = NULL; \
	}

#define bhash_alloc(result, is_new, table, key, config) \
	do { \
		bhash_index_t bhash__len = table != NULL ? table->base.len : 0; \
		bhash_index_t bhash__exp = table != NULL ? table->base.exp : 0; \
		bhash_index_t bhash__capacity = table != NULL ? (1 << (bhash_exp - 1)) : 0; \
		bhash_index_t bhash__extra_space = config.removable ? 1 : 0; /* Extra temp space for swapping */ \
		/* Grow */ \
		if (bhash__len == bhash__capacity) { \
			if (table == NULL) { \
				/* Init */ \
				bhash__exp = BHASH_INITIAL_EXP; \
				bhash__capacity = 1 << (bhash__exp - 1); \
				table = BHASH_REALLOC(NULL, sizeof(*table), config.memctx); \
				table->keys = BHASH_REALLOC(NULL, sizeof(table->keys[0]) * bhash__capacity + bhash__extra_space, config.memctx); \
				if (config->has_values) { \
					table->values = BHASH_REALLOC(NULL, sizeof(table->values[0]) * bhash__capacity + bhash__extra_space, config.memctx); \
				} else { \
					table->values = NULL; \
				} \
				table->base.hashes = BHASH_REALLOC(NULL, sizeof(bhash_hash_t) * bhash__capacity, config.memctx); \
				if (config->removable) { \
					table->base.r_indices = BHASH_REALLOC(NULL, sizeof(bhash_index_t) * bhash__capacity, config.memctx); \
				} else { \
					table->base.r_indices = NULL; \
				} \
				table->base.indices = BHASH_REALLOC(NULL, sizeof(bhash_index_t) * bhash__capacity * 2, config.memctx); \
				memset(table->base.indices, 0, sizeof(bhash_index_t) * bhash__capacity * 2); \
				table->base.len = 0; \
			} else { \
				/* Grow storage */ \
				bhash__exp += 1; \
				bhash__capacity = 1 << (bhash__exp - 1); \
				table->keys = BHASH_REALLOC(table->keys, sizeof(table->keys[0]) * bhash__capacity + bhash__extra_space, config.memctx); \
				if (config->has_values) { \
					table->values = BHASH_REALLOC(table->values, sizeof(table->values[0]) * bhash__capacity + bhash__extra_space, config.memctx); \
				} \
				bhash_hash_t* bhash__hashes = table->base.hashes = BHASH_REALLOC(table->base.hashes, sizeof(bhash_hash_t) * bhash__capacity, config.memctx); \
				bhash_index_t* bhash__indices = table->base.indices = BHASH_REALLOC(table->base.indices, sizeof(bhash_index_t) * bhash__capacity * 2, config.memctx); \
				memset(bhash__indices, 0, sizeof(bhash_index_t) * bhash__capacity * 2); \
				bhash_index_t* bhash__r_indices = NULL; \
				if (config->removable) { \
					bhash__r_indices = table->base.r_indices = BHASH_REALLOC(table->base.r_indices, sizeof(bhash_index_t) * bhash__capacity, config.memctx); \
				} \
				/* Rehash everything */ \
				for (bhash_index_t bhash__i = 0; bhash__i < bhash__len; ++bhash__i) { \
					bhash_hash_t bhash__hash = bhash__hashes[bhash__i]; \
					for (bhash_index_t bhash__j = (bhash_index_t)bhash__hash;;) { \
						bhash__j = bhash_lookup_index(bhash__hash, bhash__exp, bhash__j); \
						if (bhash__indices[bhash__j] == BHASH_EMPTY) { \
							bhash__indices[bhash__j] = bhash__i + 1; /* Offset by 1 since 0 means empty */ \
							if (config.removable) { bhash__r_indices[bhash__i] = bhash__j; } \
							break; \
						} \
					} \
				} \
			} \
			table->base.exp = bhash__exp; \
		} \
		bhash_hash_t bhash__hash = config.hash(&key, sizeof(key)); \
		bhash_index_t bhash__dest_slot = -1; \
		bhash_index_t* bhash__indices = table->base.indices; \
		bhash_index_t* bhash__r_indices = table->base.r_indices; \
		for (bhash_index_t bhash__i = (bhash_index_t)bhash__hash;;) { \
			bhash__i = bhash_lookup_index(bhash__hash, bhash__exp, bhash__i); \
			bhash_index_t bhash__index = bhash__indices[bhash__i]; \
			if (bhash__index == BHASH_EMPTY) { \
				bhash__dest_slot = bhash__dest_slot == -1 ? bhash__i : bhash__dest_slot; \
				result = table->base.len++; \
				bhash__indices[bhash__dest_slot] = result + 1; \
				if (config.removable) { bhash__r_indices[result] = bhash__dest_slot; } \
				table->base.hashes[result] = bhash__hash; \
				is_new = true; \
				break; \
			} else if (bhash__index == BHASH_TOMBSTONE) { \
				bhash__dest_slot = bhash__dest_slot == -1 ? bhash__i : bhash__dest_slot; \
			} else if (config.eq(&key, &table->base.keys[bhash__index - 1], sizeof(key))) { \
				result = bhash__index - 1; \
				is_new = false; \
				break; \
			} \
		} \
	} while (0)

#define bhash_put(table, key, value, config) \
	do { \
		bhash_index_t bhash__index; \
		bool bhash__is_new; \
		(void)bhash__is_new; \
		bhash_alloc(bhash__index, bhash__is_new, table, key, config); \
		table->keys[bhash__index] = key; \
		if (config.has_values) { table->values[bhash__index] = value; } \
	} while (0)

#define bhash_is_valid(index) ((index) >= 0)

#define bhash_remove(index, table, key, config) \
	if (table != NULL && config.removable) { \
		bhash_index_t bhash__index, bhash__r_index; \
		bhash_index_t bhash__end_index = table->base.len; \
		bhash_index_t bhash__tail_index = bhash__end_index - 1; \
		bhash__find_impl(bhash__index, bhash__r_index, table, key, config); \
		if (bhash_is_valid(bhash__index)) { \
			/* Move the last element into the deleted slot and delete the last element */ \
			bhash_index_t bhash__tail_r_index = table->base.r_indices[bhash__tail_index]; \
			table->base.indices[bhash__tail_r_index] = bhash__index + 1; \
			table->base.indices[bhash__r_index] = BHASH_TOMBSTONE; \
			table->base.r_indices[bhash__index] = bhash__tail_r_index; \
			/* Rotate key and values then point user code to the temp position at the end */ \
			table->keys[bhash__end_index] = table->keys[bhash__index]; \
			table->keys[bhash__index] = table->keys[bhash__tail_index]; \
			if (config.has_values) { \
				table->values[bhash__end_index] = table->values[bhash__index]; \
				table->values[bhash__index] = table->values[bhash__tail_index]; \
			} \
			table->base.len -= 1; \
			index = bhash__end_index; \
		} else { \
			index = -1; \
		} \
	} else { \
		index = -1; \
	}

#define bhash_find(index, table, key, config) \
	do { \
		bhash_index_t bhash__r_index; \
		(void)bhash__r_index; \
		bhash__find_impl(index, bhash__r_index, table, key, config); \
	} while (0)

#define bhash_len(table) ((table)->base.len)

#define bhash_keys(table) ((table)->keys)

#define bhash_values(table) ((table)->values)

#define bhash__find_impl(index, r_index, table, key, config) \
	if (table != NULL) { \
		bhash_hash_t bhash__hash = config.hash(&key, sizeof(key)); \
		bhash_index_t* bhash__indices = table->base.indices; \
		bhash_index_t bhash__exp = table->base.exp; \
		for (bhash_index_t bhash__i = (bhash_index_t)bhash__hash;;) { \
			bhash__i = bhash_lookup_index(bhash__hash, bhash__exp, bhash__i); \
			bhash_index_t bhash__index = bhash__indices[bhash__i]; \
			if (bhash__index == BHASH_EMPTY) { \
				index = r_index = -1; \
				break; \
			} else if (bhash__index == BHASH_TOMBSTONE) { \
				continue; \
			} else if (config.eq(&key, &table->base.keys[bhash__index - 1], sizeof(key))) { \
				index = bhash__index - 1; \
				r_index = bhash__i; \
				break; \
			} \
		} \
	} else { \
		index = r_index = -1; \
	}

// MurmurOAAT64
static inline bhash_hash_t
bhash_hash(const void* key, size_t size) {
	bhash_hash_t h = 525201411107845655ull;
	for (size_t i = 0; i < size; ++i) {
		h ^= ((const unsigned char*)key)[i];
		h *= 0x5bd1e9955bd1e995ull;
		h ^= h >> 47;
	}
	return h;
}

// https://nullprogram.com/blog/2022/08/08/
static inline bhash_index_t
bhash_lookup_index(bhash_hash_t hash, bhash_index_t exp, bhash_index_t idx) {
	uint32_t mask = ((uint32_t)1 << exp) - 1;
	uint32_t step = (hash >> (64 - exp)) | 1;
	return (idx + step) & mask;
}

static inline bool
bhash_eq(const void* lhs, const void* rhs, size_t size) {
	return memcmp(lhs, rhs, size) == 0;
}

static inline bhash_hash_t
bhash_str_hash(const void* key, size_t size) {
	(void)size;
	return bhash_hash(key, strlen(key));
}

static inline bool
bhash_str_eq(const void* lhs, const void* rhs, size_t size) {
	(void)size;
	return strcmp(lhs, rhs);
}

#ifndef BHASH_REALLOC
#include <stdlib.h>

#define BHASH_REALLOC(ptr, size, ctx) bhash__libc_realloc(ptr, size, ctx)

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

#endif
