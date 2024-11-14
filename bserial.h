#ifndef BSERIAL_H
#define BSERIAL_H

/**
 * @file
 *
 * @brief Serialization library.
 *
 * There are several API groups:
 *
 * * Low level I/O: Read/write data against an abstract stream in an endian-independent way.
 * * Stream implemenation:
 *   * BSERIAL_MEM: A memory stream.
 *   * BSERIAL_STDIO: Wrapper for FILE in stdio.h.
 * * Structured data: Read/write structured data with backward-compatibility.
 *   Keys in a record can be added/removed/reordered.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#ifndef BSERIAL_API
#define BSERIAL_API
#endif

/*! How many bytes to skip at a time */
#ifndef BSERIAL_SKIP_BLKSIZE
#define BSERIAL_SKIP_BLKSIZE 1024
#endif

/*! Helper macro to check for IO status and return on error */
#define BSERIAL_CHECK_STATUS(OP) \
	do { \
		bserial_status_t status = OP; \
		if (status != BSERIAL_OK) { return status; } \
	} while(0)

/*! IO status */
typedef enum {
	/*! No error */
	BSERIAL_OK,
	/*! IO error */
	BSERIAL_IO_ERROR,
	/*! Malformed data encountered */
	BSERIAL_MALFORMED,
} bserial_status_t;

/*! Abstract input stream */
typedef struct bserial_in_s {
	/**
	 * @brief Read from the stream.
	 *
	 * @param in The input stream.
	 * @param buf The buffer to read into.
	 * @param size How many bytes to read.
	 * @return Number of bytes read.
	 */
	size_t (*read)(struct bserial_in_s* in, void* buf, size_t size);

	/**
	 * @brief Skip a number of bytes (optional).
	 *
	 * @param in The input stream.
	 * @param size Number of bytes to skip.
	 * @return Whether the operation was successful.
	 */
	bool (*skip)(struct bserial_in_s* in, size_t size);
} bserial_in_t;

/*! Abstract output stream */
typedef struct bserial_out_s {
	/**
	 * @brief Write to the stream.
	 *
	 * @param out The out stream.
	 * @param buf The buffer to write.
	 * @param size How many bytes to write.
	 * @return Number of bytes written.
	 */
	size_t (*write)(struct bserial_out_s* out, const void* buf, size_t size);
} bserial_out_t;

/**
 * @brief Serialization mode.
 *
 * @see bserial_make_ctx
 */
typedef enum {
	/*! Writing */
	BSERIAL_MODE_WRITE,
	/*! Reading */
	BSERIAL_MODE_READ,
} bserial_mode_t;

/**
 * @brief Serialization context.
 *
 * @see bserial_make_ctx
 */
typedef struct bserial_ctx_s bserial_ctx_t;

/**
 * @brief Serialization context configuration.
 *
 * @see bserial_make_ctx
 */
typedef struct bserial_ctx_config_s {
	/**
	 * @brief Maximum length of each symbol
	 * @see bserial_symbol
	 */
	uint32_t max_symbol_len;
	/**
	 * @brief Maximum number of symbols
	 * @see bserial_symbol
	 */
	uint32_t max_num_symbols;
	/**
	 * @brief Maximum number fields per record.
	 * @see bserial_record
	 */
	uint32_t max_record_fields;
	/**
	 * @brief Maximum nested depth.
	 * @see bserial_record
	 * @see bserial_array
	 * @see bserial_table
	 */
	uint32_t max_depth;
} bserial_ctx_config_t;

/**
 * @brief Debug trace callback
 *
 * @param depth The depth of the serialization context.
 * @param fmt Format string.
 * @param args Arguments to pass to a vprintf-like function.
 * @param userdata Arbitrary userdata
 *
 * @see bserial_trace
 */
typedef void (*bserial_tracer_t)(int depth, const char* fmt, va_list args, void* userdata);

#ifdef __cplusplus
extern "C" {
#endif

// Stream utilities

static inline bserial_status_t
bserial_read(bserial_in_t* in, void* buf, size_t size) {
	char* cbuf = buf;
	while (size > 0) {
		size_t bytes_read = in->read(in, cbuf, size);
		if (bytes_read == 0) { return BSERIAL_IO_ERROR; }
		cbuf += bytes_read;
		size -= bytes_read;
	}

	return BSERIAL_OK;
}

static inline bserial_status_t
bserial_skip(bserial_in_t* in, size_t size) {
	if (in->skip) {
		return in->skip(in, size) ? BSERIAL_OK : BSERIAL_IO_ERROR;
	} else {
		char buf[BSERIAL_SKIP_BLKSIZE];
		while (size > 0) {
			size_t read_size = BSERIAL_SKIP_BLKSIZE > size ? BSERIAL_SKIP_BLKSIZE : size;
			BSERIAL_CHECK_STATUS(bserial_read(in, buf, read_size));
			size -= read_size;
		}
		return BSERIAL_OK;
	}
}

static inline bserial_status_t
bserial_write(bserial_out_t* out, const void* buf, size_t size) {
	const char* cbuf = buf;
	while (size > 0) {
		size_t bytes_written = out->write(out, cbuf, size);
		if (bytes_written == 0) { return BSERIAL_IO_ERROR; }
		cbuf += bytes_written;
		size -= bytes_written;
	}

	return BSERIAL_OK;
}

// Primitives

/*! Write an unsigned varint */
BSERIAL_API bserial_status_t
bserial_write_uint(uint64_t x, bserial_out_t* out);

/*! Read an unsigned varint */
BSERIAL_API bserial_status_t
bserial_read_uint(uint64_t* x, bserial_in_t* in);

/*! Write a signed varint */
BSERIAL_API bserial_status_t
bserial_write_sint(int64_t x, bserial_out_t* out);

/*! Read a signed varint */
BSERIAL_API bserial_status_t
bserial_read_sint(int64_t* x, bserial_in_t* in);

/*! Write a single precision float */
BSERIAL_API bserial_status_t
bserial_write_f32(float f32, bserial_out_t* out);

/*! Read a single precision float */
BSERIAL_API bserial_status_t
bserial_read_f32(float* f32, bserial_in_t* in);

/*! Write a double precision float */
BSERIAL_API bserial_status_t
bserial_write_f64(double f64, bserial_out_t* out);

/*! Read a double precision float */
BSERIAL_API bserial_status_t
bserial_read_f64(double* f64, bserial_in_t* in);

/*! Write a string */
BSERIAL_API bserial_status_t
bserial_write_str(const char* str, uint64_t len, bserial_out_t* out);

/**
 * @brief Read a string
 * @param buf The buffer to read into.
 * @param len_inout Size of the buffer.
 *   Will be set to the actual length excluding the null terminator.
 * @param in The input stream.
 */
BSERIAL_API bserial_status_t
bserial_read_str(char* buf, uint64_t* len_inout, bserial_in_t* in);

// Structured data

/*! How much memory is required for the serialization context */
BSERIAL_API size_t
bserial_ctx_mem_size(bserial_ctx_config_t config);

/**
 * @brief Initialize a serialization context
 * @param mem Memory for the context.
 *   Must have at least as many bytes as returned by @ref bserial_ctx_mem_size.
 * @param in (Optional) Input stream.
 * @param out (Optional) Out stream.
 *
 * @remarks
 *   Either @a in or @a out must be provided.
 */
BSERIAL_API bserial_ctx_t*
bserial_make_ctx(
	void* mem,
	bserial_ctx_config_t config,
	bserial_in_t* in,
	bserial_out_t* out
);

/*! Get the current serialization mode */
BSERIAL_API bserial_mode_t
bserial_mode(bserial_ctx_t* ctx);

/*! Get the current IO status */
BSERIAL_API bserial_status_t
bserial_status(bserial_ctx_t* ctx);

/**
 * @brief Read/write an unsigned varint
 *
 * @see bserial_any_int
 */
BSERIAL_API bserial_status_t
bserial_uint(bserial_ctx_t* ctx, uint64_t* value);

/**
 * @brief Read/write a signed varint
 *
 * @see bserial_any_int
 */
BSERIAL_API bserial_status_t
bserial_sint(bserial_ctx_t* ctx, int64_t* value);

/*! Read/write a single precision float */
BSERIAL_API bserial_status_t
bserial_f32(bserial_ctx_t* ctx, float* value);

/*! Read/write a single double float */
BSERIAL_API bserial_status_t
bserial_f64(bserial_ctx_t* ctx, double* value);

// Int type adapters

/** @see bserial_any_int */
BSERIAL_API bserial_status_t
bserial_i8(bserial_ctx_t* ctx, int8_t* i8);

/** @see bserial_any_int */
BSERIAL_API bserial_status_t
bserial_i16(bserial_ctx_t* ctx, int16_t* i16);

/** @see bserial_any_int */
BSERIAL_API bserial_status_t
bserial_i32(bserial_ctx_t* ctx, int32_t* i32);

/** @see bserial_any_int */
BSERIAL_API bserial_status_t
bserial_u8(bserial_ctx_t* ctx, uint8_t* u8);

/** @see bserial_any_int */
BSERIAL_API bserial_status_t
bserial_u16(bserial_ctx_t* ctx, uint16_t* u16);

/** @see bserial_any_int */
BSERIAL_API bserial_status_t
bserial_u32(bserial_ctx_t* ctx, uint32_t* u32);

/**
 * @brief Automatically select the right integer serialization function
 *
 * For types smaller than 64 bits, bound check is also performed.
 */
#define bserial_any_int(ctx, integer) \
	_Generic( \
		integer, \
		int8_t*: bserial_i8, \
		int16_t*: bserial_i16, \
		int32_t*: bserial_i32, \
		int64_t*: bserial_sint, \
		uint8_t*: bserial_u8, \
		uint16_t*: bserial_u16, \
		uint32_t*: bserial_u32, \
		uint64_t*: bserial_uint \
	)(ctx, integer)

/**
 * @brief Read/write a binary blob
 *
 * Instead of combining into one step, one can also use: @ref bserial_blob_header
 * and @ref bserial_blob_body.
 *
 * @param buf The buffer to read/write.
 * @param len Size of the buffer. Will be set to the actual size.
 */
BSERIAL_API bserial_status_t
bserial_blob(bserial_ctx_t* ctx, char* buf, uint64_t* len);

/**
 * @brief Read/write a binary blob's header
 *
 * @param len Maximum size. Will be set to the actual size on read.
 */
BSERIAL_API bserial_status_t
bserial_blob_header(bserial_ctx_t* ctx, uint64_t* len);

/*! @brief Read/write a binary blob's body */
BSERIAL_API bserial_status_t
bserial_blob_body(bserial_ctx_t* ctx, char* buf);

/**
 * @brief Read/write a symbol.
 *
 * @param buf Pointer to the buffer to read/write.
 * @param len Size of the buffer. Will be set to the actual size.
 * @remarks
 *   Symbols are interned.
 *   When the same symbol is written multiple times, only a single copy is
 *   written to the stream.
 *   Subsequent writes will only refer to its unique id.
 *
 * @remarks
 *   @a buf is a pointer to a pointer since upon reading or writing, it will be
 *   adjusted to point to the internal symbol buffer.
 */
BSERIAL_API bserial_status_t
bserial_symbol(bserial_ctx_t* ctx, const char** buf, uint64_t* len);

/**
 * @brief Read/write an array.
 *
 * After this call @a len elements are expected.
 */
BSERIAL_API bserial_status_t
bserial_array(bserial_ctx_t* ctx, uint64_t* len);

/**
 * @brief Read/write a table.
 *
 * After this call @a len elements are expected.
 *
 * This is a special case of an array where all members must be records of the same type.
 *
 * @see bserial_record
 */
BSERIAL_API bserial_status_t
bserial_table(bserial_ctx_t* ctx, uint64_t* len);

/**
 * @brief Read/write a record.
 *
 * A record is a collection of key/value pairs.
 *
 * The library needs to make several passes over the structure of the record.
 * This should always be called as the condition of a while loop:
 * `while (bserial_record(ctx, record)) {`.
 * Therefore, the macro @ref BSERIAL_RECORD should be used.
 *
 * @param ctx The serialization context.
 * @param record Address of the record being serialized.
 *   This is needed to differentiate between nested records.
 *
 * @see bserial_key
 */
BSERIAL_API bool
bserial_record(bserial_ctx_t* ctx, void* record);

/**
 * @brief Read/write a key.
 *
 * This can only be called within a @ref BSERIAL_RECORD block.
 *
 * The input data may have extra keys, missing keys or out of order keys.
 * Therefore, the value serialization code should only be run if this returns true.
 * This function should always be used in a condition: `if (bserial_key(ctx, name, len) {`.
 *
 * Moreover, @a name should be a constant.
 *
 * Therefore, the @ref BSERIAL_KEY macro should be used.
 *
 * @param name Name of the field being serialized.
 * @param len Length of the field name being serialized.
 */
BSERIAL_API bool
bserial_key(bserial_ctx_t* ctx, const char* name, uint64_t len);

/**
 * @brief Read/write a record>
 *
 * @param ctx The serialization context.
 * @param record Address of the record being serialized.
 *   This is needed to differentiate between nested records.
 */
#define BSERIAL_RECORD(ctx, record) while (bserial_record(ctx, record))

/**
 * Read/write a key in a record
 *
 * @param ctx The serialization context.
 * @param name Literal name of a record field, without any quote.
 *   e.g: `foo` and **not** `"foo"`.
 */
#define BSERIAL_KEY(ctx, name) if (bserial_key(ctx, #name, sizeof(#name) - 1))

/*! Trace the error context during serialization */
BSERIAL_API void
bserial_trace(bserial_ctx_t* ctx, bserial_tracer_t tracer, void* userdata);

#ifdef BSERIAL_STDIO

#include <stdio.h>

/*! stdio input stream */
typedef struct bserial_stdio_in_s {
	bserial_in_t bserial;
	FILE* file;
} bserial_stdio_in_t;

/*! stdio output stream */
typedef struct bserial_stdio_out_s {
	bserial_out_t bserial;
	FILE* file;
} bserial_stdio_out_t;

/*! Wrap a stdio FILE into an input stream */
BSERIAL_API bserial_in_t*
bserial_stdio_init_in(bserial_stdio_in_t* bserial_stdio, FILE* file);

/*! Wrap a stdio FILE into an output stream */
BSERIAL_API bserial_out_t*
bserial_stdio_init_out(bserial_stdio_out_t* bserial_stdio, FILE* file);

#endif

#ifdef BSERIAL_MEM

/*! Memory input stream */
typedef struct bserial_mem_in_s {
	bserial_in_t bserial;
	char* cur;
	char* end;
} bserial_mem_in_t;

/*! Memory output stream */
typedef struct bserial_mem_out_s {
	bserial_out_t bserial;
	/*! Size of bserial_mem_out_t.mem */
	size_t len;
	size_t capacity;
	void* memctx;
	/**
	 * @brief The underlying memory.
	 *
	 * This should be manually freed.
	 */
	char* mem;
} bserial_mem_out_t;

/**
 * @brief Create a memory input stream
 * @param mem The backing memory.
 * @param size Size of the backing memory.
 * @return An input stream.
 */
BSERIAL_API bserial_in_t*
bserial_mem_init_in(bserial_mem_in_t* bserial_mem, void* mem, size_t size);

/**
 * @brief Create a memory output stream
 * @param memctx The allocator context.
 * @return An output stream.
 * @see bserial_mem_out_t
 * @remarks
 *   bserial_mem_out_t.mem should be manually freed.
 */
BSERIAL_API bserial_out_t*
bserial_mem_init_out(bserial_mem_out_t* bserial_mem, void* memctx);

#endif

#ifdef __cplusplus
}
#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSERIAL_IMPLEMENTATION)
#define BSERIAL_IMPLEMENTATION
#endif

#ifdef BSERIAL_IMPLEMENTATION

#include <string.h>
#include <inttypes.h>
#include "mem_layout.h"

bserial_status_t
bserial_write_uint(uint64_t x, bserial_out_t* out) {
    char buf[10];
    size_t n = 0;

	for (int i = 0; i < 10; ++i) {
		n += x >= 0x80;
		buf[i] = (char)(x | 0x80);
		x >>= 7;
	}

    buf[n] ^= 0x80;
	n += 1;

	return bserial_write(out, buf, n);
}

bserial_status_t
bserial_write_sint(int64_t x, bserial_out_t* out) {
    uint64_t ux = (uint64_t)x << 1;
    if (x < 0) { ux = ~ux; }
    return bserial_write_uint(ux, out);
}

bserial_status_t
bserial_read_uint(uint64_t* x, bserial_in_t* in) {
	uint64_t b;
	char c;
	uint64_t tmp = 0;

	for (int i = 0; i < 10; ++i) {
		BSERIAL_CHECK_STATUS(bserial_read(in, &c, 1));

		b = c;
		tmp |= (b & 0x7f) << (7 * i);
		if (b < 0x80) {
			*x = tmp;
			return BSERIAL_OK;
		}
	}

	return BSERIAL_MALFORMED;
}

bserial_status_t
bserial_read_sint(int64_t* x, bserial_in_t* in) {
    uint64_t ux;

	BSERIAL_CHECK_STATUS(bserial_read_uint(&ux, in));

    int64_t tmp = (int64_t)(ux >> 1);
    if ((ux & 1) != 0) {
		tmp = ~tmp;
    }

	*x = tmp;
    return BSERIAL_OK;
}

bserial_status_t
bserial_write_f32(float f32, bserial_out_t* out) {
	uint32_t ivalue;
	memcpy(&ivalue, &f32, sizeof(f32));

	uint8_t buf[sizeof(ivalue)];
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		buf[i] = (uint8_t)(ivalue >> (i * 8));
	}

	return bserial_write(out, buf, sizeof(buf));
}

bserial_status_t
bserial_read_f32(float* f32, bserial_in_t* in) {
	uint32_t ivalue = 0;

	uint8_t buf[sizeof(ivalue)];
	BSERIAL_CHECK_STATUS(bserial_read(in, buf, sizeof(buf)));
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		ivalue |= (uint32_t)buf[i] << (i * 8);
	}
	memcpy(f32, &ivalue, sizeof(ivalue));

	return BSERIAL_OK;
}

bserial_status_t
bserial_write_f64(double f64, bserial_out_t* out) {
	uint64_t ivalue;
	memcpy(&ivalue, &f64, sizeof(f64));

	uint8_t buf[sizeof(ivalue)];
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		buf[i] = (uint8_t)(ivalue >> (i * 8));
	}

	return bserial_write(out, buf, sizeof(buf));
}

bserial_status_t
bserial_read_f64(double* f64, bserial_in_t* in) {
	uint64_t ivalue = 0;

	uint8_t buf[sizeof(ivalue)];
	BSERIAL_CHECK_STATUS(bserial_read(in, buf, sizeof(buf)));
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		ivalue |= (uint64_t)buf[i] << (i * 8);
	}
	memcpy(f64, &ivalue, sizeof(ivalue));

	return BSERIAL_OK;
}

bserial_status_t
bserial_write_str(const char* str, uint64_t len, bserial_out_t* out) {
	BSERIAL_CHECK_STATUS(bserial_write_uint(len, out));
	BSERIAL_CHECK_STATUS(bserial_write(out, str, len));

	return BSERIAL_OK;
}

bserial_status_t
bserial_read_str(
	char* buf,
	uint64_t* len_inout,
	bserial_in_t* in
) {
	uint64_t length;

	BSERIAL_CHECK_STATUS(bserial_read_uint(&length, in));
	if (length > *len_inout) { return BSERIAL_MALFORMED; }
	if (length > 0) { BSERIAL_CHECK_STATUS(bserial_read(in, buf, length)); }

	*len_inout = length;
	return BSERIAL_OK;
}

typedef enum {
	BSERIAL_UINT         =  1,
	BSERIAL_SINT         =  2,
	BSERIAL_F32          =  3,
	BSERIAL_F64          =  4,
	BSERIAL_BLOB         =  5,
	BSERIAL_SYM_DEF      =  6,
	BSERIAL_SYM_REF      =  7,
	BSERIAL_ARRAY        =  8,
	BSERIAL_TABLE        =  9,
	BSERIAL_RECORD       = 10,
} bserial_marker_t;

typedef enum {
	BSERIAL_SCOPE_ROOT,
	BSERIAL_SCOPE_BLOB,
	BSERIAL_SCOPE_ARRAY,
	BSERIAL_SCOPE_TABLE,
	BSERIAL_SCOPE_RECORD,
} bserial_scope_type_t;

typedef enum {
	BSERIAL_OP_NUMERIC,
	BSERIAL_OP_BLOB,
	BSERIAL_OP_SYMBOL,
	BSERIAL_OP_TABLE,
	BSERIAL_OP_ARRAY,
	BSERIAL_OP_RECORD,
} bserial_op_type_t;

typedef enum {
	BSERIAL_RECORD_MEASURE_WIDTH,
	BSERIAL_RECORD_KEY_IO,
	BSERIAL_RECORD_VALUE_IO,
} bserial_record_mode_t;

typedef struct {
	char* buf;
	uint64_t len;
} bserial_symbol_t;

typedef struct {
	const char* symbol;
	uint64_t symbol_len;
	const char* field_name;
} bserial_record_mapping_t;

typedef struct {
	bserial_scope_type_t type;

	uint64_t iterator;
	uint64_t len;

	bserial_record_mode_t record_mode;
	bserial_record_mapping_t* record_schema;
	bserial_record_mapping_t* prev_schema_pool;
	void* record_addr;
	uint64_t record_width;
} bserial_scope_t;

struct bserial_ctx_s {
	bserial_ctx_config_t config;
	bserial_status_t status;
	bserial_in_t* in;
	bserial_out_t* out;
	uint8_t marker_buf;

	bserial_symbol_t* symtab;
	uint32_t num_symbols;
	int32_t* symtab_index;
	int32_t symtab_exp;
	char* strpool;

	int32_t key_exp;
	bserial_scope_t* scope_first;
	bserial_scope_t* scope;
	bserial_scope_t* scope_last;
	bserial_record_mapping_t* schema_pool;
};

static inline size_t
bserial_ctx_mem_layout(void* mem, bserial_ctx_config_t config) {
	mem_layout_t layout = { 0 };
	mem_layout_reserve(&layout, sizeof(bserial_ctx_t), _Alignof(bserial_ctx_t));

	ptrdiff_t symtab = mem_layout_reserve(
		&layout,
		sizeof(bserial_symbol_t) * config.max_num_symbols,
		_Alignof(bserial_symbol_t)
	);

	int32_t symtab_exp = 2;
	while (((int32_t)1 << symtab_exp) < (int32_t)(config.max_num_symbols * 2)) {
		++symtab_exp;
	}
	int32_t symtab_index_len = ((int32_t)1 << symtab_exp);
	ptrdiff_t symtab_index = mem_layout_reserve(
		&layout,
		sizeof(int32_t) * symtab_index_len,
		_Alignof(int32_t)
	);
	ptrdiff_t schema_pool = mem_layout_reserve(
		&layout,
		sizeof(bserial_record_mapping_t) * config.max_depth * config.max_record_fields,
		_Alignof(bserial_record_mapping_t)
	);

	ptrdiff_t scope = mem_layout_reserve(
		&layout,
		sizeof(bserial_scope_t) * config.max_depth,
		_Alignof(bserial_scope_t)
	);

	ptrdiff_t strpool = mem_layout_reserve(
		&layout,
		sizeof(char) * (config.max_symbol_len + 1) * config.max_num_symbols,
		_Alignof(char)
	);

	if (mem) {
		bserial_ctx_t* ctx = mem;
		ctx->symtab = mem_layout_locate(mem, symtab);

		ctx->symtab_index = mem_layout_locate(mem, symtab_index);
		ctx->symtab_exp = symtab_exp;
		memset(ctx->symtab_index, 0, sizeof(*ctx->symtab_index) * symtab_index_len);

		ctx->scope_first = ctx->scope = mem_layout_locate(mem, scope);
		ctx->scope_last = ctx->scope + config.max_depth - 1;
		ctx->scope->type = BSERIAL_SCOPE_ROOT;
		ctx->scope->prev_schema_pool = ctx->schema_pool;

		ctx->strpool = mem_layout_locate(mem, strpool);
		ctx->schema_pool = mem_layout_locate(mem, schema_pool);
		ctx->marker_buf = UINT8_MAX;
	}

	return mem_layout_size(&layout);
}

size_t
bserial_ctx_mem_size(bserial_ctx_config_t config) {
	return bserial_ctx_mem_layout(NULL, config);
}

bserial_ctx_t*
bserial_make_ctx(
	void* mem,
	bserial_ctx_config_t config,
	bserial_in_t* in,
	bserial_out_t* out
) {
	bserial_ctx_t* ctx = mem;
	*ctx = (bserial_ctx_t) {
		.config = config,
		.status = BSERIAL_OK,
		.in = in,
		.out = out,
	};

	bserial_ctx_mem_layout(mem, config);

	return mem;
}

bserial_mode_t
bserial_mode(bserial_ctx_t* ctx) {
	return ctx->in != NULL ? BSERIAL_MODE_READ : BSERIAL_MODE_WRITE;
}

bserial_status_t
bserial_status(bserial_ctx_t* ctx) {
	return ctx->status;
}

static inline bserial_status_t
bserial_malformed(bserial_ctx_t* ctx) {
	return ctx->status = BSERIAL_MALFORMED;
}

static inline bserial_status_t
bserial_push_scope(bserial_ctx_t* ctx, bserial_scope_type_t type) {
	BSERIAL_CHECK_STATUS(ctx->status);
	if (ctx->scope == ctx->scope_last) {
		return bserial_malformed(ctx);
	}

	*(++ctx->scope) = (bserial_scope_t){
		.type = type,
		.prev_schema_pool = ctx->schema_pool,
	};

	if (type == BSERIAL_SCOPE_RECORD || type == BSERIAL_SCOPE_TABLE) {
		ctx->scope->record_schema = ctx->schema_pool;
		ctx->schema_pool += ctx->config.max_record_fields;
	}

	return BSERIAL_OK;
}

static inline bserial_status_t
bserial_pop_scope(bserial_ctx_t* ctx) {
	BSERIAL_CHECK_STATUS(ctx->status);
	if (ctx->scope->type == BSERIAL_SCOPE_ROOT) {
		return bserial_malformed(ctx);
	}

	ctx->schema_pool = ctx->scope->prev_schema_pool;
	--ctx->scope;
	return BSERIAL_OK;
}

static inline bserial_status_t
bserial_begin_op(bserial_ctx_t* ctx, bserial_op_type_t op) {
	BSERIAL_CHECK_STATUS(ctx->status);

	bserial_scope_t* scope = ctx->scope;
	bserial_scope_type_t scope_type = scope->type;

	// Can't do anything before filling the blob
	if (scope_type == BSERIAL_SCOPE_BLOB) {
		return bserial_malformed(ctx);
	}

	if (
		scope_type == BSERIAL_SCOPE_TABLE
		&& op != BSERIAL_OP_RECORD
	) {
		return bserial_malformed(ctx);
	}

	// Count the number of elements
	if (
		scope_type == BSERIAL_SCOPE_ARRAY
		|| scope_type == BSERIAL_SCOPE_TABLE
	) {
		++scope->iterator;
	}

	if (op == BSERIAL_OP_BLOB) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_BLOB));
	} else if (op == BSERIAL_OP_ARRAY) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_ARRAY));
	} else if (op == BSERIAL_OP_TABLE) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_TABLE));
	} else if (op == BSERIAL_OP_RECORD) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_RECORD));
	}

	return BSERIAL_OK;
}

static inline bserial_status_t
bserial_end_op(bserial_ctx_t* ctx, bserial_op_type_t op) {
	BSERIAL_CHECK_STATUS(ctx->status);

	// Record can have any nested bserial_end_op within its value but only the
	// bserial_end_op that ends its own op should pop the scope.
	if (
		(ctx->scope->type == BSERIAL_SCOPE_BLOB && op == BSERIAL_OP_BLOB)
		|| (ctx->scope->type == BSERIAL_SCOPE_RECORD && op == BSERIAL_OP_RECORD)
	) {
		BSERIAL_CHECK_STATUS(bserial_pop_scope(ctx));
	}

	// Auto pop when enough ops are executed.
	// Array and table do not have an "end" function call and the number of
	// elements are automatically tracked through bserial_end_op.
	while (
		(ctx->scope->type == BSERIAL_SCOPE_ARRAY
		 || ctx->scope->type == BSERIAL_SCOPE_TABLE)
		&& ctx->scope->iterator == ctx->scope->len
	) {
		BSERIAL_CHECK_STATUS(bserial_pop_scope(ctx));
	}

	return BSERIAL_OK;
}

#define BSERIAL_NO_MARKER ((uint8_t)UINT8_MAX)

static inline bserial_status_t
bserial_peek_marker(bserial_ctx_t* ctx, uint8_t* marker) {
	if (ctx->marker_buf == BSERIAL_NO_MARKER) {
		BSERIAL_CHECK_STATUS(ctx->status = bserial_read(ctx->in, &ctx->marker_buf, sizeof(uint8_t)));
	}

	*marker = ctx->marker_buf;
	return BSERIAL_OK;
}

static inline bserial_status_t
bserial_read_marker(bserial_ctx_t* ctx, uint8_t* marker) {
	if (ctx->marker_buf == BSERIAL_NO_MARKER) {
		return ctx->status = bserial_read(ctx->in, marker, sizeof(uint8_t));
	} else {
		*marker = ctx->marker_buf;
		ctx->marker_buf = BSERIAL_NO_MARKER;
		return BSERIAL_OK;
	}
}

static inline void
bserial_discard_marker(bserial_ctx_t* ctx) {
	ctx->marker_buf = BSERIAL_NO_MARKER;
}

bserial_status_t
bserial_uint(bserial_ctx_t* ctx, uint64_t* value) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_NUMERIC));

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		uint8_t marker;
		BSERIAL_CHECK_STATUS(bserial_read_marker(ctx, &marker));
		if (marker != BSERIAL_UINT) { return bserial_malformed(ctx); }

		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(value, ctx->in));
	} else {
		uint8_t marker = BSERIAL_UINT;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write_uint(*value, ctx->out));
	}

	return bserial_end_op(ctx, BSERIAL_OP_NUMERIC);
}

bserial_status_t
bserial_sint(bserial_ctx_t* ctx, int64_t* value) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_NUMERIC));

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		uint8_t marker;
		BSERIAL_CHECK_STATUS(bserial_read_marker(ctx, &marker));
		if (marker != BSERIAL_SINT) { return bserial_malformed(ctx); }

		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_sint(value, ctx->in));
	} else {
		uint8_t marker = BSERIAL_SINT;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write_sint(*value, ctx->out));
	}

	return bserial_end_op(ctx, BSERIAL_OP_NUMERIC);
}

bserial_status_t
bserial_i8(bserial_ctx_t* ctx, int8_t* i8) {
	int64_t i64 = *i8;
	BSERIAL_CHECK_STATUS(bserial_sint(ctx, &i64));

	if ((int64_t)INT8_MIN <= i64 && i64 <= (int64_t)INT8_MAX) {
		*i8 = (int8_t)i64;
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

bserial_status_t
bserial_i16(bserial_ctx_t* ctx, int16_t* i16) {
	int64_t i64 = *i16;
	BSERIAL_CHECK_STATUS(bserial_sint(ctx, &i64));

	if ((int64_t)INT16_MIN <= i64 && i64 <= (int64_t)INT16_MAX) {
		*i16 = (int16_t)i64;
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

bserial_status_t
bserial_i32(bserial_ctx_t* ctx, int32_t* i32) {
	int64_t i64 = *i32;
	BSERIAL_CHECK_STATUS(bserial_sint(ctx, &i64));

	if ((int64_t)INT32_MIN <= i64 && i64 <= (int64_t)INT32_MAX) {
		*i32 = (int32_t)i64;
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

bserial_status_t
bserial_u8(bserial_ctx_t* ctx, uint8_t* u8) {
	uint64_t u64 = *u8;
	BSERIAL_CHECK_STATUS(bserial_uint(ctx, &u64));

	if (u64 <= (uint64_t)UINT8_MAX) {
		*u8 = (uint8_t)u64;
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

bserial_status_t
bserial_u16(bserial_ctx_t* ctx, uint16_t* u16) {
	uint64_t u64 = *u16;
	BSERIAL_CHECK_STATUS(bserial_uint(ctx, &u64));

	if (u64 <= (uint64_t)UINT16_MAX) {
		*u16 = (uint16_t)u64;
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

bserial_status_t
bserial_u32(bserial_ctx_t* ctx, uint32_t* u32) {
	uint64_t u64 = *u32;
	BSERIAL_CHECK_STATUS(bserial_uint(ctx, &u64));

	if (u64 <= (uint64_t)UINT32_MAX) {
		*u32 = (uint32_t)u64;
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

bserial_status_t
bserial_f32(bserial_ctx_t* ctx, float* value) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_NUMERIC));

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		uint8_t marker;
		BSERIAL_CHECK_STATUS(bserial_read_marker(ctx, &marker));
		if (marker != BSERIAL_F32) { return bserial_malformed(ctx); }

		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_f32(value, ctx->in));
	} else {
		uint8_t marker = BSERIAL_F32;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write_f32(*value, ctx->out));
	}

	return bserial_end_op(ctx, BSERIAL_OP_NUMERIC);
}

bserial_status_t
bserial_f64(bserial_ctx_t* ctx, double* value) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_NUMERIC));

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		uint8_t marker;
		BSERIAL_CHECK_STATUS(bserial_read_marker(ctx, &marker));
		if (marker != BSERIAL_F64) { return bserial_malformed(ctx); }

		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_f64(value, ctx->in));
	} else {
		uint8_t marker = BSERIAL_F64;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write_f64(*value, ctx->out));
	}

	return bserial_end_op(ctx, BSERIAL_OP_NUMERIC);
}

static inline bserial_status_t
bserial_marker_and_length(bserial_ctx_t* ctx, uint8_t marker, uint64_t* length) {
	BSERIAL_CHECK_STATUS(ctx->status);

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		uint8_t actual_marker;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_read(ctx->in, &actual_marker, sizeof(actual_marker)));
		if (actual_marker != marker) { return bserial_malformed(ctx); }

		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(length, ctx->in));
	} else {
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
		BSERIAL_CHECK_STATUS(ctx->status = bserial_write_uint(*length, ctx->out));
	}

	return BSERIAL_OK;
}

bserial_status_t
bserial_blob(bserial_ctx_t* ctx, char* buf, uint64_t* len) {
	uint64_t actual_len = *len;
	BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &actual_len));
	if (actual_len > *len) { return bserial_malformed(ctx); }
	*len = actual_len;

	BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, buf));

	return BSERIAL_OK;
}

bserial_status_t
bserial_blob_header(bserial_ctx_t* ctx, uint64_t* len) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_BLOB));
	BSERIAL_CHECK_STATUS(ctx->status = bserial_marker_and_length(ctx, BSERIAL_BLOB, len));

	ctx->scope->len = *len;

	return BSERIAL_OK;
}

bserial_status_t
bserial_blob_body(bserial_ctx_t* ctx, char* buf) {
	BSERIAL_CHECK_STATUS(ctx->status);

	if (ctx->scope->type != BSERIAL_SCOPE_BLOB) {
		return bserial_malformed(ctx);
	}

	if (ctx->scope->len > 0) {
		if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
			BSERIAL_CHECK_STATUS(ctx->status = bserial_read(ctx->in, buf, ctx->scope->len));
		} else {
			BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, buf, ctx->scope->len));
		}
	}

	return bserial_end_op(ctx, BSERIAL_OP_BLOB);
}

// MurmurOAAT64
static inline uint64_t
bserial_hash(const void* key, size_t size) {
	uint64_t h = 525201411107845655ull;
	for (size_t i = 0; i < size; ++i) {
		h ^= ((const unsigned char*)key)[i];
		h *= 0x5bd1e9955bd1e995ull;
		h ^= h >> 47;
	}
	return h;
}

// https://nullprogram.com/blog/2022/08/08/
static inline int32_t
bserial_lookup_index(uint64_t hash, int32_t exp, int32_t idx) {
	uint32_t mask = ((uint32_t)1 << exp) - 1;
	uint32_t step = (uint32_t)((hash >> (64 - exp)) | 1);
	return (idx + step) & mask;
}

static inline bserial_status_t
bserial_skip_next(bserial_ctx_t* ctx, uint32_t depth) {
	uint8_t marker;
	BSERIAL_CHECK_STATUS(bserial_peek_marker(ctx, &marker));

	switch ((bserial_marker_t)marker) {
		case BSERIAL_UINT:
			{
				uint64_t u64;
				BSERIAL_CHECK_STATUS(bserial_uint(ctx, &u64));
			}
			break;
		case BSERIAL_SINT:
			{
				int64_t s64;
				BSERIAL_CHECK_STATUS(bserial_sint(ctx, &s64));
			}
			break;
		case BSERIAL_F32:
			{
				bserial_discard_marker(ctx);
				BSERIAL_CHECK_STATUS(ctx->status = bserial_skip(ctx->in, sizeof(float)));
			}
			break;
		case BSERIAL_F64:
			{
				bserial_discard_marker(ctx);
				BSERIAL_CHECK_STATUS(ctx->status = bserial_skip(ctx->in, sizeof(double)));
			}
			break;
		case BSERIAL_BLOB:
			{
				bserial_discard_marker(ctx);
				uint64_t len;
				BSERIAL_CHECK_STATUS(bserial_read_uint(&len, ctx->in));
				BSERIAL_CHECK_STATUS(ctx->status = bserial_skip(ctx->in, len));
			}
			break;
		case BSERIAL_SYM_DEF:
		case BSERIAL_SYM_REF:
			{
				const char* sym;
				uint64_t sym_len;
				BSERIAL_CHECK_STATUS(bserial_symbol(ctx, &sym, &sym_len));
			}
			break;
		case BSERIAL_ARRAY:
			{
				bserial_discard_marker(ctx);

				uint64_t len;
				BSERIAL_CHECK_STATUS(bserial_read_uint(&len, ctx->in));
				if (len > 0 && depth == 0) { return bserial_malformed(ctx); }

				for (uint64_t i = 0; i < len; ++i) {
					BSERIAL_CHECK_STATUS(bserial_skip_next(ctx, depth - 1));
				}
			}
			break;
		case BSERIAL_TABLE:
			{
				bserial_discard_marker(ctx);

				uint64_t num_rows;
				BSERIAL_CHECK_STATUS(bserial_read_uint(&num_rows, ctx->in));
				if (num_rows > 0 && depth == 0) { return bserial_malformed(ctx); }

				uint64_t num_cols;
				BSERIAL_CHECK_STATUS(bserial_read_uint(&num_cols, ctx->in));

				for (uint64_t i = 0; i < num_cols; ++i) {
					const char* sym;
					uint64_t sym_len;
					BSERIAL_CHECK_STATUS(bserial_symbol(ctx, &sym, &sym_len));
				}

				for (uint64_t i = 0; i < num_rows; ++i) {
					for (uint64_t j = 0; j < num_cols; ++j) {
						BSERIAL_CHECK_STATUS(bserial_skip_next(ctx, depth - 1));
					}
				}
			}
			break;
		case BSERIAL_RECORD:
			{
				bserial_discard_marker(ctx);

				uint64_t num_cols;
				BSERIAL_CHECK_STATUS(bserial_read_uint(&num_cols, ctx->in));

				for (uint64_t i = 0; i < num_cols; ++i) {
					const char* sym;
					uint64_t sym_len;
					BSERIAL_CHECK_STATUS(bserial_symbol(ctx, &sym, &sym_len));
				}

				for (uint64_t i = 0; i < num_cols; ++i) {
					BSERIAL_CHECK_STATUS(bserial_skip_next(ctx, depth - 1));
				}
			}
			break;
		default:
			return bserial_malformed(ctx);
	}

	return BSERIAL_OK;
}

static inline bool
bserial_probe_next_record_field(bserial_ctx_t* ctx) {
	bserial_scope_t* scope = ctx->scope;

	while (scope->iterator < scope->len) {
		if (scope->record_schema[scope->iterator].field_name != NULL) {
			return true;
		} else {
			if (
				bserial_skip_next(
					ctx,
					ctx->config.max_depth - (uint32_t)(ctx->scope - ctx->scope_first)
				) != BSERIAL_OK
			) {
				return false;
			}
			++scope->iterator;
		}
	}

	bserial_end_op(ctx, BSERIAL_OP_RECORD);
	return false;
}

bserial_status_t
bserial_symbol(bserial_ctx_t* ctx, const char** buf, uint64_t* len) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_SYMBOL));

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		uint8_t marker;
		BSERIAL_CHECK_STATUS(bserial_read_marker(ctx, &marker));

		if (marker == BSERIAL_SYM_DEF) {
			if (ctx->num_symbols >= ctx->config.max_num_symbols) { return bserial_malformed(ctx); }

			uint64_t symbol_len;
			BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&symbol_len, ctx->in));
			if (symbol_len > ctx->config.max_symbol_len) { return bserial_malformed(ctx); }

			BSERIAL_CHECK_STATUS(ctx->status = bserial_read(ctx->in, ctx->strpool, symbol_len));
			ctx->strpool[symbol_len] = '\0';

			ctx->symtab[ctx->num_symbols] = (bserial_symbol_t){
				.buf = ctx->strpool,
				.len = symbol_len,
			};

			*buf = ctx->strpool;
			*len = symbol_len;

			ctx->num_symbols += 1;
			ctx->strpool += (symbol_len + 1);
		} else if (marker == BSERIAL_SYM_REF) {
			uint64_t symbol_id;
			BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&symbol_id, ctx->in));
			if (symbol_id >= ctx->num_symbols) { return bserial_malformed(ctx); }

			*buf = ctx->symtab[symbol_id].buf;
			*len = ctx->symtab[symbol_id].len;
		} else {
			return bserial_malformed(ctx);
		}
	} else {
		uint64_t symbol_len = *len;
		if (symbol_len > ctx->config.max_symbol_len) { return bserial_malformed(ctx); }

		uint64_t symbol_hash = bserial_hash(*buf, symbol_len);
		for (int32_t i = (int32_t)symbol_hash;;) {
			i = bserial_lookup_index(symbol_hash, ctx->symtab_exp, i);
			int32_t index = ctx->symtab_index[i];
			if (index == 0) {
				if (ctx->num_symbols >= ctx->config.max_num_symbols) {
					return bserial_malformed(ctx);
				}
				memcpy(ctx->strpool, *buf, symbol_len);
				ctx->strpool[symbol_len] = '\0';

				ctx->symtab[ctx->num_symbols] = (bserial_symbol_t){
					.buf = ctx->strpool,
					.len = symbol_len,
				};
				ctx->symtab_index[i] = ctx->num_symbols + 1;
				ctx->num_symbols += 1;
				ctx->strpool += (symbol_len + 1);

				uint8_t marker = BSERIAL_SYM_DEF;
				BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
				BSERIAL_CHECK_STATUS(ctx->status = bserial_write_uint(symbol_len, ctx->out));
				BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, *buf, symbol_len));

				break;
			} else if (
				ctx->symtab[index - 1].len == symbol_len
				&& memcmp(ctx->symtab[index - 1].buf, *buf, symbol_len) == 0
			) {
				uint8_t marker = BSERIAL_SYM_REF;
				BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
				BSERIAL_CHECK_STATUS(ctx->status = bserial_write_uint((uint64_t)index - 1, ctx->out));

				break;
			}
		}
	}

	return bserial_end_op(ctx, BSERIAL_OP_SYMBOL);
}

bserial_status_t
bserial_array(bserial_ctx_t* ctx, uint64_t* len) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_ARRAY));
	BSERIAL_CHECK_STATUS(bserial_marker_and_length(ctx, BSERIAL_ARRAY, len));

	if (*len > 0) {
		ctx->scope->len = *len;
		return BSERIAL_OK;
	} else {
		return bserial_end_op(ctx, BSERIAL_OP_ARRAY);
	}
}

bserial_status_t
bserial_table(bserial_ctx_t* ctx, uint64_t* len) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_TABLE));
	BSERIAL_CHECK_STATUS(bserial_marker_and_length(ctx, BSERIAL_TABLE, len));

	if (*len > 0) {
		ctx->scope->len = *len;
		return BSERIAL_OK;
	} else {
		return bserial_end_op(ctx, BSERIAL_OP_TABLE);
	}
}

bool
bserial_record(bserial_ctx_t* ctx, void* record) {
	if (ctx->status != BSERIAL_OK) { return false; }

	bserial_scope_t* scope = ctx->scope;

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		if (scope->type == BSERIAL_SCOPE_RECORD && record == scope->record_addr) {
			switch (scope->record_mode) {
				case BSERIAL_RECORD_KEY_IO:
					scope->record_mode = BSERIAL_RECORD_VALUE_IO;
					scope->iterator = 0;
					return bserial_probe_next_record_field(ctx);
				case BSERIAL_RECORD_VALUE_IO:
					if (scope->iterator == scope->len) {
						bserial_end_op(ctx, BSERIAL_OP_RECORD);
						return false;
					} else {
						return bserial_probe_next_record_field(ctx);
					}
				default:
					bserial_malformed(ctx);
					return false;
			}
		} else {
			bserial_scope_t* parent_scope = scope;
			if (bserial_begin_op(ctx, BSERIAL_OP_RECORD) != BSERIAL_OK) {
				return false;
			}
			scope = ctx->scope;
			scope->record_addr = record;

			if (parent_scope->type != BSERIAL_SCOPE_TABLE) {
				uint8_t marker;
				if ((bserial_read_marker(ctx, &marker)) != BSERIAL_OK) {
					return false;
				}
				if (marker != BSERIAL_RECORD) {
					bserial_malformed(ctx);
					return false;
				}
			}

			// In case of a table, allocate schema at the table's scope.
			bserial_scope_t* schema_scope =
				parent_scope->type == BSERIAL_SCOPE_TABLE
					? parent_scope
					: scope;

			// Schema discovery only happens for the first row of a table
			if (parent_scope->type != BSERIAL_SCOPE_TABLE || parent_scope->iterator == 1) {
				scope->record_mode = BSERIAL_RECORD_KEY_IO;

				uint64_t num_fields;
				if ((ctx->status = bserial_read_uint(&num_fields, ctx->in)) != BSERIAL_OK) {
					return false;
				}
				if (num_fields > ctx->config.max_record_fields) {
					bserial_malformed(ctx);
					return false;
				}

				for (uint64_t i = 0; i < num_fields; ++i) {
					schema_scope->record_schema[i].field_name = NULL;
				}

				schema_scope->record_width = num_fields;
				for (uint64_t i = 0; i < num_fields; ++i) {
					const char* symbol;
					uint64_t symbol_len;
					scope->iterator = i;
					if (bserial_symbol(ctx, &symbol, &symbol_len) != BSERIAL_OK) {
						return false;
					}

					schema_scope->record_schema[i].symbol = symbol;
					schema_scope->record_schema[i].symbol_len = symbol_len;
				}
				scope->iterator = 0;
			} else {
				scope->record_mode = BSERIAL_RECORD_VALUE_IO;
			}

			scope->record_schema = schema_scope->record_schema;
			scope->len = schema_scope->record_width;

			if (scope->record_mode == BSERIAL_RECORD_VALUE_IO) {
				return bserial_probe_next_record_field(ctx);
			} else {
				return true;
			}
		}
	} else {
		if (scope->type == BSERIAL_SCOPE_RECORD && scope->record_addr == record) {
			switch (scope->record_mode) {
				case BSERIAL_RECORD_MEASURE_WIDTH:
					scope->record_mode = BSERIAL_RECORD_KEY_IO;
					ctx->status = bserial_write_uint(scope->len, ctx->out);
					return ctx->status == BSERIAL_OK;
				case BSERIAL_RECORD_KEY_IO:
					scope->record_mode = BSERIAL_RECORD_VALUE_IO;
					scope->iterator = 0;
					return true;
				case BSERIAL_RECORD_VALUE_IO:
					bserial_end_op(ctx, BSERIAL_OP_RECORD);
					return false;
				default:
					bserial_malformed(ctx);
					return false;
			}
		} else {
			bserial_scope_t* parent_scope = scope;
			if (bserial_begin_op(ctx, BSERIAL_OP_RECORD) != BSERIAL_OK) {
				return false;
			}
			scope = ctx->scope;
			scope->record_addr = record;

			if (parent_scope->type != BSERIAL_SCOPE_TABLE || parent_scope->iterator == 1) {
				scope->record_mode = BSERIAL_RECORD_MEASURE_WIDTH;

				if (parent_scope->type != BSERIAL_SCOPE_TABLE) {
					uint8_t marker = BSERIAL_RECORD;
					if ((ctx->status = bserial_write(ctx->out, &marker, sizeof(marker))) != BSERIAL_OK) {
						return false;
					}
				}
			} else {
				scope->record_mode = BSERIAL_RECORD_VALUE_IO;
			}

			return true;
		}
	}
}

bool
bserial_key(bserial_ctx_t* ctx, const char* name, uint64_t len) {
	if (ctx->status != BSERIAL_OK) { return false; }

	bserial_scope_t* scope = ctx->scope;

	if (scope->type != BSERIAL_SCOPE_RECORD) {
		bserial_malformed(ctx);
		return false;
	}

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		switch (scope->record_mode) {
			case BSERIAL_RECORD_KEY_IO:
				for (uint64_t i = 0; i < scope->len; ++i) {
					if (
						scope->record_schema[i].symbol_len == len
						&& strncmp(scope->record_schema[i].symbol, name, len) == 0
					) {
						scope->record_schema[i].field_name = name;
					}
				}
				++scope->iterator;
				return false;
			case BSERIAL_RECORD_VALUE_IO:
				if (name == scope->record_schema[scope->iterator].field_name) {
					++scope->iterator;
					return true;
				} else {
					return false;
				}
			default:
				bserial_malformed(ctx);
				return false;
		}
	} else {
		switch (scope->record_mode) {
			case BSERIAL_RECORD_MEASURE_WIDTH:
				++scope->len;
				return false;
			case BSERIAL_RECORD_KEY_IO:
				++scope->iterator;
				bserial_symbol(ctx, &name, &len);
				return false;
			case BSERIAL_RECORD_VALUE_IO:
				++scope->iterator;
				return true;
			default:
				bserial_malformed(ctx);
				return false;
		}
	}
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 4, 5)))
#endif
static inline void
bserial_tracef(bserial_tracer_t tracer, void* userdata, int depth, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	tracer(depth, fmt, args, userdata);
	va_end(args);
}

void
bserial_trace(bserial_ctx_t* ctx, bserial_tracer_t tracer, void* userdata) {
	for (bserial_scope_t* scope = ctx->scope_first; scope <= ctx->scope; ++scope) {
		int depth = (int)(scope - ctx->scope_first);
		switch (scope->type) {
			case BSERIAL_SCOPE_ROOT:
				bserial_tracef(tracer, userdata, depth, "Root");
				break;
			case BSERIAL_SCOPE_ARRAY:
				bserial_tracef(
					tracer, userdata, depth,
					"Array(%" PRIu64 "/%" PRIu64 ")", scope->iterator, scope->len
				);
				break;
			case BSERIAL_SCOPE_TABLE:
				bserial_tracef(
					tracer, userdata, depth,
					"Table(%" PRIu64 "/%" PRIu64 ")", scope->iterator, scope->len
				);
				break;
			case BSERIAL_SCOPE_RECORD:
				bserial_tracef(
					tracer, userdata, depth,
					"Record(%" PRIu64 "/%" PRIu64 ") (Phase %d)",
					scope->iterator, scope->len, scope->record_mode
				);
				break;
			case BSERIAL_SCOPE_BLOB:
				bserial_tracef(tracer, userdata, depth, "Blob(%" PRIu64 ")", scope->len);
				break;
		}
	}
}

#ifdef BSERIAL_STDIO

static inline size_t
bserial_stdio_read(bserial_in_t* in, void* buf, size_t size) {
	return fread(buf, size, 1, ((bserial_stdio_in_t*)in)->file) == 1 ? size : 0;
}

static inline bool
bserial_stdio_skip(bserial_in_t* in, size_t size) {
	return fseek(((bserial_stdio_in_t*)in)->file, size, SEEK_CUR) == 0;
}

static inline size_t
bserial_stdio_write(bserial_out_t* out, const void* buf, size_t size) {
	return fwrite(buf, size, 1, ((bserial_stdio_out_t*)out)->file) == 1 ? size : 0;
}

bserial_in_t*
bserial_stdio_init_in(bserial_stdio_in_t* bserial_stdio, FILE* file) {
	*bserial_stdio = (bserial_stdio_in_t) {
		.bserial = {
			.read = bserial_stdio_read,
			.skip = bserial_stdio_skip,
		},
		.file = file,
	};
	return &bserial_stdio->bserial;
}

bserial_out_t*
bserial_stdio_init_out(bserial_stdio_out_t* bserial_stdio, FILE* file) {
	*bserial_stdio = (bserial_stdio_out_t) {
		.bserial.write = bserial_stdio_write,
		.file = file,
	};
	return &bserial_stdio->bserial;
}

#endif

#ifdef BSERIAL_MEM

#ifndef BSERIAL_REALLOC
#	ifdef BLIB_REALLOC
#		define BSERIAL_REALLOC BLIB_REALLOC
#	else
#		define BSERIAL_REALLOC(ptr, size, ctx) bserial_libc_realloc(ptr, size)
#		define BSERIAL_USE_LIBC_REALLOC
#	endif
#endif

#ifdef BSERIAL_USE_LIBC_REALLOC

#include <stdlib.h>

static inline void*
bserial_libc_realloc(void* ptr, size_t size) {
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

static inline size_t
bserial_mem_read(bserial_in_t* in, void* buf, size_t size) {
	bserial_mem_in_t* mem_in = (bserial_mem_in_t*)in;

	if (mem_in->cur + size <= mem_in->end) {
		memcpy(buf, mem_in->cur, size);
		mem_in->cur += size;
		return size;
	} else {
		return 0;
	}
}

static inline bool
bserial_mem_skip(bserial_in_t* in, size_t size) {
	bserial_mem_in_t* mem_in = (bserial_mem_in_t*)in;

	if (mem_in->cur + size <= mem_in->end) {
		mem_in->cur += size;
		return true;
	} else {
		return false;
	}
}

static inline size_t
bserial_mem_write(bserial_out_t* out, const void* buf, size_t size) {
	bserial_mem_out_t* mem_out = (bserial_mem_out_t*)out;

	size_t required_capacity = mem_out->len + size;
	size_t capacity = mem_out->capacity;
	if (required_capacity > capacity) {
		size_t double_capacity = capacity * 2;
		size_t new_capacity = double_capacity > required_capacity
			? double_capacity
			: required_capacity;

		mem_out->mem = BSERIAL_REALLOC(mem_out->mem, new_capacity, mem_out->memctx);
		if (mem_out->mem == NULL) { return 0; }

		mem_out->capacity = new_capacity;
	}

	memcpy(mem_out->mem + mem_out->len, buf, size);
	mem_out->len += size;
	return size;
}

bserial_in_t*
bserial_mem_init_in(bserial_mem_in_t* bserial_mem, void* mem, size_t size) {
	*bserial_mem = (bserial_mem_in_t){
		.bserial = {
			.read = bserial_mem_read,
			.skip = bserial_mem_skip,
		},
		.cur = mem,
		.end = (char*)mem + size,
	};
	return &bserial_mem->bserial;
}

bserial_out_t*
bserial_mem_init_out(bserial_mem_out_t* bserial_mem, void* memctx) {
	*bserial_mem = (bserial_mem_out_t){
		.bserial.write = bserial_mem_write,
		.len = 0,
		.capacity = 0,
		.mem = NULL,
		.memctx = memctx,
	};
	return &bserial_mem->bserial;
}

#endif

#endif
