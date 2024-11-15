#ifndef BSERIAL_TEST_RECORD_H
#define BSERIAL_TEST_RECORD_H

#include "../../bserial.h"
#include <string.h>

typedef struct {
	float x;
	float y;
} vec2f_t;

typedef struct {
	int num;
	char str[256];

	int array_len;
	int array[8];

	vec2f_t vec2f;

	int table_len;
	vec2f_t table[8];
} original_t;

static inline bserial_status_t
serialize_vec2f(bserial_ctx_t* ctx, vec2f_t* rec) {
	BSERIAL_RECORD(ctx, rec) {
		BSERIAL_KEY(ctx, x) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->x));
		}

		BSERIAL_KEY(ctx, y) {
			BSERIAL_CHECK_STATUS(bserial_f32(ctx, &rec->y));
		}
	}

	return bserial_status(ctx);
}

static inline bserial_status_t
serialize_original(bserial_ctx_t* ctx, original_t* rec) {
	BSERIAL_RECORD(ctx, rec) {
		BSERIAL_KEY(ctx, num) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->num));
		}

		BSERIAL_KEY(ctx, str) {
			uint64_t len = strlen(rec->str);
			BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));
			if (len >= sizeof(rec->str)) { return BSERIAL_MALFORMED; }
			BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, rec->str));
			rec->str[len] = '\0';
		}

		BSERIAL_KEY(ctx, array) {
			uint64_t len = (uint64_t)rec->array_len;
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
			if (len >= (sizeof(rec->array) / sizeof(rec->array[0]))) {
				return BSERIAL_MALFORMED;
			}
			rec->array_len = (int)len;
			for (int i = 0; i < rec->array_len; ++i) {
				BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->array[i]));
			}
		}

		BSERIAL_KEY(ctx, vec2f) {
			BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->vec2f));
		}

		BSERIAL_KEY(ctx, table) {
			uint64_t len = (uint64_t)rec->table_len;
			BSERIAL_CHECK_STATUS(bserial_table(ctx, &len));
			if (len >= (sizeof(rec->table) / sizeof(rec->table[0]))) {
				return BSERIAL_MALFORMED;
			}
			rec->table_len = (int)len;
			for (int i = 0; i < rec->table_len; ++i) {
				BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->table[i]));
			}
		}
	}

	return bserial_status(ctx);
}

static inline bserial_status_t
serialize_original_flipped(bserial_ctx_t* ctx, original_t* rec) {
	// Order of keys does not matter
	BSERIAL_RECORD(ctx, rec) {
		BSERIAL_KEY(ctx, str) {
			uint64_t len = strlen(rec->str);
			BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));
			if (len >= sizeof(rec->str)) { return BSERIAL_MALFORMED; }
			BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, rec->str));
			rec->str[len] = '\0';
		}

		BSERIAL_KEY(ctx, table) {
			uint64_t len = (uint64_t)rec->table_len;
			BSERIAL_CHECK_STATUS(bserial_table(ctx, &len));
			if (len >= (sizeof(rec->table) / sizeof(rec->table[0]))) {
				return BSERIAL_MALFORMED;
			}
			rec->table_len = (int)len;
			for (int i = 0; i < rec->table_len; ++i) {
				BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->table[i]));
			}
		}

		BSERIAL_KEY(ctx, vec2f) {
			BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->vec2f));
		}

		BSERIAL_KEY(ctx, array) {
			uint64_t len = (uint64_t)rec->array_len;
			BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
			if (len >= (sizeof(rec->array) / sizeof(rec->array[0]))) {
				return BSERIAL_MALFORMED;
			}
			rec->array_len = (int)len;
			for (int i = 0; i < rec->array_len; ++i) {
				BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->array[i]));
			}
		}

		BSERIAL_KEY(ctx, num) {
			BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->num));
		}
	}

	return bserial_status(ctx);
}

static inline bserial_status_t
serialize_original_skip(bserial_ctx_t* ctx, original_t* rec, int selector) {
	// Depending on the selector, only 1 of the 5 fields will be deserialized.
	BSERIAL_RECORD(ctx, rec) {
		if (selector == 0) {
			BSERIAL_KEY(ctx, str) {
				uint64_t len = strlen(rec->str);
				BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));
				if (len >= sizeof(rec->str)) { return BSERIAL_MALFORMED; }
				BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, rec->str));
				rec->str[len] = '\0';
			}
		}

		if (selector == 1) {
			BSERIAL_KEY(ctx, array) {
				uint64_t len = (uint64_t)rec->array_len;
				BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
				if (len >= (sizeof(rec->array) / sizeof(rec->array[0]))) {
					return BSERIAL_MALFORMED;
				}
				rec->array_len = (int)len;
				for (int i = 0; i < rec->array_len; ++i) {
					BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->array[i]));
				}
			}
		}

		if (selector == 2) {
			BSERIAL_KEY(ctx, num) {
				BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &rec->num));
			}
		}

		if (selector == 3) {
			BSERIAL_KEY(ctx, vec2f) {
				BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->vec2f));
			}
		}

		if (selector == 4) {
			BSERIAL_KEY(ctx, table) {
				uint64_t len = (uint64_t)rec->table_len;
				BSERIAL_CHECK_STATUS(bserial_table(ctx, &len));
				if (len >= (sizeof(rec->table) / sizeof(rec->table[0]))) {
					return BSERIAL_MALFORMED;
				}
				rec->table_len = (int)len;
				for (int i = 0; i < rec->table_len; ++i) {
					BSERIAL_CHECK_STATUS(serialize_vec2f(ctx, &rec->table[i]));
				}
			}
		}
	}

	return bserial_status(ctx);
}

#endif
