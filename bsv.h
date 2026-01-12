#ifndef BSV_H
#define BSV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file
 *
 * @brief Compact binary serialization library with versioning.
 *
 * This is inspired by [Media Module serialization system](https://handmade.network/p/29/swedish-cubes-for-unity/blog/p/2723-how_media_molecule_does_serialization).
 * However, instead of using a global version, it opts to do versioning at each object/record.
 * This allows the serialization function to be more composable and make it harder to forget to increment the version.
 *
 * As long as the library is used as intended, a newer version is guaranteed to be able to read data from all older versions.
 * However, it will not be able to write data for older versions.
 *
 * There are several API groups:
 *
 * * Low level I/O: Read/write data against an abstract stream in an endian-independent way.
 * * Stream implemenation:
 *   * BSV_MEM: A memory stream.
 *   * BSV_STDIO: Wrapper for FILE in stdio.h.
 * * Structured data: Read/write structured data with backward-compatibility.
 */

#ifndef BSV_API
#define BSV_API
#endif

/**
 * Define a list of fundamental types for @ref bsv_auto
 *
 * This is an [X macro](https://en.wikipedia.org/wiki/X_macro) that maps a type
 * to a serialization function.
 *
 * This should be done before including bsv.h.
 * Then each type here can be simply serialized using @ref bsv_auto.
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_CUSTOM_TYPES
 *
 * @hideinitializer
 */
#ifndef BSV_CUSTOM_TYPES
#define BSV_CUSTOM_TYPES(X) \
	/* X(my_custom_type, bsv_my_custom_type) */
#endif

/*! Customizable version type */
#ifndef BSV_VERSION_TYPE
#define BSV_VERSION_TYPE uint32_t
#endif

#ifndef BSV_ASSERT
#include <assert.h>
#define BSV_ASSERT(cond) assert(cond)
#endif

/*! Helper macro to check for IO status and return on error */
#define BSV_CHECK_STATUS(OP) \
	do { \
		bsv_status_t bsv__status = OP; \
		if (bsv__status != BSV_OK) { return bsv__status; } \
	} while(0)

/**
 * Create a versioned block.
 *
 * This should be at the root of a serialization function.
 *
 * @param CTX The current serialization context
 * @param LATEST_VERSION The latest version number.
 *   This should be incremented every time fields are added or removed from the block.
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_BLK
 *
 * @hideinitializer
 */
#define BSV_BLK(CTX, LATEST_VERSION) \
	for ( \
		bsv_blk_ctx_t bsv__blk_ctx = (BSV__SET_EXPLAIN_INFO((CTX)), bsv_begin_block((CTX), (LATEST_VERSION))); \
		bsv__blk_ctx.counter < 1; \
		bsv_end_block(bsv__blk_ctx.bsv, &bsv__blk_ctx), ++bsv__blk_ctx.counter \
	) \

/**
 * Create a block where the pointer to a @ref bsv_ctx_t is implicit.
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_ARRAY
 *
 * @hideinitializer
 */
#define BSV_WITH(CTX) \
	for ( \
		bsv_blk_ctx_t bsv__blk_ctx = { .bsv = (CTX) }; \
		bsv__blk_ctx.counter < 1; \
		++bsv__blk_ctx.counter \
	) \

/**
 * Refer to the current context within a @ref BSV_WITH block.
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_ARRAY
 *
 * @hideinitializer
 */
#define BSV_CTX (bsv__blk_ctx.bsv)

/**
 * Define a revision.
 *
 * Within each @ref BSV_BLK block, there is a series of `BSV_REV` blocks.
 * Each `BSV_REV` block defines a new revision with a series of @ref BSV_ADD or @ref BSV_REM.
 *
 * @param REVISION The revision number.
 *   Must be monotonically increasing.
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_REV
 *
 * @hideinitializer
 */
#define BSV_REV(REVISION) \
	for ( \
		int BSV__##__LINE__ = (BSV__SET_EXPLAIN_INFO((BSV_CTX)), bsv_begin_revision(BSV_CTX, (REVISION)), 0); \
		BSV__##__LINE__ < 1; \
		++BSV__##__LINE__, bsv_end_revision(BSV_CTX) \
	)

/**
 * Serialize an array.
 *
 * @param CTX Pointer to the current @ref bsv_ctx_t
 * @param LEN Pointer to a value of type @ref bsv_len_t
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_ARRAY
 *
 * @hideinitializer
 */
#define BSV_ARRAY(CTX, LEN) \
	for ( \
		bsv_array_ctx_t BSV__##__LINE__ = (BSV__SET_EXPLAIN_INFO(CTX), bsv_begin_array(CTX, (LEN))); \
		BSV__##__LINE__.counter < 1; \
		++BSV__##__LINE__.counter, bsv_end_array(CTX, &BSV__##__LINE__) \
	) \

/**
 * Add a field to the serialization.
 *
 * @param PTR Pointer to a value
 *
 * @see BSV_ADD_EX
 * @see bsv_auto
 */
#define BSV_ADD(PTR) BSV_ADD_EX(PTR, bsv_auto)

/**
 * Add a field to the serialization.
 *
 * This must be nested within a @ref BSV_BLK and a @ref BSV_REV
 *
 * @param PTR Pointer to a value
 * @param SERIALIZER A serializer function
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_BLK
 *
 * @hideinitializer
 */
#define BSV_ADD_EX(PTR, SERIALIZER) \
	do { \
		if (bsv_should_serialize_add(BSV_CTX)) { \
			BSV__SET_EXPLAIN_INFO(BSV_CTX); \
			BSV_CTX->explain_info.name = #PTR; \
			bsv_trace_begin(BSV_CTX, BSV_EXPLAIN_ADD); \
			SERIALIZER(BSV_CTX, PTR); \
			bsv_trace_end(BSV_CTX, BSV_EXPLAIN_ADD); \
		} \
	} while (0)

/**
 * Remove a previously added field.
 *
 * @param PTR Pointer to a value
 * @param VERSION_REMOVED The version that removes this field
 *
 * @see BSV_REM_EX
 * @see bsv_auto
 */
#define BSV_REM(PTR, VERSION_REMOVED) BSV_REM_EX(PTR, bsv_auto, VERSION_REMOVED)

/**
 * Remove a previously added field.
 *
 * This must be nested within a @ref BSV_BLK and a @ref BSV_REV.
 *
 * To remove a field, instead of adding a new @ref BSV_REV, edit the @ref BSV_ADD
 * that previously introduced the field into a `BSV_REM`.
 * This should be followed by a code block that converts the old data into a newer format.
 *
 * @param PTR Pointer to a value
 * @param SERIALIZER The serialization function
 * @param VERSION_REMOVED The version that removes this field
 *
 * Example:
 *
 * @snippet samples/bsv.c  BSV_REM
 *
 * @hideinitializer
 */
#define BSV_REM_EX(PTR, SERIALIZER, VERSION_REMOVED) \
	if (bsv_should_serialize_rem(BSV_CTX, (VERSION_REMOVED))) { \
		BSV__SET_EXPLAIN_INFO(BSV_CTX); \
		BSV_CTX->explain_info.name = #PTR; \
		BSV_CTX->explain_info.version = VERSION_REMOVED; \
		bsv_trace_begin(BSV_CTX, BSV_EXPLAIN_REM); \
		SERIALIZER(BSV_CTX, PTR); \
		bsv_trace_end(BSV_CTX, BSV_EXPLAIN_REM); \
	} \
	if (bsv_should_serialize_rem(BSV_CTX, (VERSION_REMOVED)))

#define BSV_EXPLAIN(TYPE, SERIALIZER, EXPLAIN_FN, EXPLAIN_CTX) \
	do { \
		TYPE bsv__explain_value = { 0 }; \
		bsv_ctx_t bsv__explain_ctx = { \
			.in = bsv__zero_in(), \
			.explain_fn = EXPLAIN_FN, \
			.explain_ctx = EXPLAIN_CTX, \
			.explain_info = { \
				.value = &bsv__explain_value, \
				.name = #SERIALIZER, \
			}, \
		}; \
		BSV__SET_EXPLAIN_INFO(&bsv__explain_ctx); \
		bsv_trace_begin(&bsv__explain_ctx, BSV_EXPLAIN_ROOT); \
		SERIALIZER(&bsv__explain_ctx, &bsv__explain_value); \
		bsv_trace_end(&bsv__explain_ctx, BSV_EXPLAIN_ROOT); \
	} while (0)

/**
 * Convenient helper to serialize fundamental types.
 *
 * Out of the box, it can read/write boolean, integer and floating point types.
 * By defining @ref BSV_CUSTOM_TYPES before including bsv.h, you can also integrate
 * more fundamental types (e.g: vector, matrix...) into this macro.
 *
 * Example:
 *
 * @snippet samples/bsv.c  bsv_auto
 *
 * @hideinitializer
 */
#define bsv_auto(CTX, PTR) \
	_Generic(*(PTR), \
		BSV_CUSTOM_TYPES(BSV__GENERIC_CLAUSE) \
		BSV__CORE_TYPES(BSV__GENERIC_CLAUSE) \
		default: bsv_unknown \
	)((CTX), (PTR))

#ifndef DOXYGEN

#define BSV__GENERIC_CLAUSE(TYPE, FN) TYPE: FN,

#define BSV__DECLARE_SERIALIZER(TYPE, FN) \
	extern bsv_status_t FN(bsv_ctx_t* ctx, TYPE* value);

#define BSV__CORE_TYPES(X) \
	X(int8_t, bsv_i8) \
	X(uint8_t, bsv_u8) \
	X(int16_t, bsv_i16) \
	X(uint16_t, bsv_u16) \
	X(int32_t, bsv_i32) \
	X(uint32_t, bsv_u32) \
	X(int64_t, bsv_sint) \
	X(uint64_t, bsv_uint) \
	X(float, bsv_f32) \
	X(double, bsv_f64) \
	X(bool, bsv_bool)

#define BSV__SET_EXPLAIN_INFO(CTX) \
	bsv_set_explain_info((CTX), __FILE__, __LINE__, __func__)

#endif

typedef struct bsv_unknown_s bsv_unkown_t;
typedef BSV_VERSION_TYPE bsv_version_t;
typedef uint64_t bsv_len_t;

/*! IO status */
typedef enum {
	/*! No error */
	BSV_OK,
	/*! IO error */
	BSV_IO_ERROR,
	/*! Malformed data encountered */
	BSV_MALFORMED,
} bsv_status_t;

/*! Abstract input stream */
typedef struct bsv_in_s {
	/**
	 * @brief Read from the stream.
	 *
	 * @param in The input stream.
	 * @param buf The buffer to read into.
	 * @param size How many bytes to read.
	 * @return Number of bytes read.
	 */
	size_t (*read)(struct bsv_in_s* in, void* buf, size_t size);
} bsv_in_t;

/*! Abstract output stream */
typedef struct bsv_out_s {
	/**
	 * @brief Write to the stream.
	 *
	 * @param out The out stream.
	 * @param buf The buffer to write.
	 * @param size How many bytes to write.
	 * @return Number of bytes written.
	 */
	size_t (*write)(struct bsv_out_s* out, const void* buf, size_t size);
} bsv_out_t;

/*! Serialization mode */
typedef enum {
	/*! Writing */
	BSV_MODE_WRITE,
	/*! Reading */
	BSV_MODE_READ,
} bsv_mode_t;

typedef enum {
	BSV_EXPLAIN_ROOT,
	BSV_EXPLAIN_BLK,
	BSV_EXPLAIN_REV,
	BSV_EXPLAIN_ARRAY,
	BSV_EXPLAIN_ADD,
	BSV_EXPLAIN_REM,
	BSV_EXPLAIN_RAW,
} bsv_explain_type_t;

typedef enum {
	BSV_EXPLAIN_BEGIN_SCOPE,
	BSV_EXPLAIN_END_SCOPE,
} bsv_explain_scope_t;

typedef struct bsv_explain_s {
	bsv_explain_type_t type;
	bsv_explain_scope_t scope;

	const char* file;
	const char* function;
	const char* name;
	int line;

	bsv_version_t version;
	void* value;
} bsv_explain_t;

typedef void (*bsv_explain_fn_t)(const bsv_explain_t* explain, void* ctx);

/*! @brief Serialization context */
typedef struct bsv_ctx_s {
	bsv_in_t* in;
	bsv_out_t* out;

	// Private do not initialize
	bsv_len_t blob_size;
	bsv_len_t array_len;
	bsv_version_t max_revision;
	bsv_version_t current_revision;
	bsv_version_t current_blk_version;
	bsv_version_t array_version;
	bsv_status_t status;

	bsv_explain_t explain_info;
	bsv_explain_fn_t explain_fn;
	void* explain_ctx;
} bsv_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

/*! Get the current serialization mode */
static inline bsv_mode_t
bsv_mode(bsv_ctx_t* ctx) {
	return ctx->in != NULL ? BSV_MODE_READ : BSV_MODE_WRITE;
}

/*! Get the current IO status */
static inline bsv_status_t
bsv_status(bsv_ctx_t* ctx) {
	return ctx->status;
}

// Stream utilities

static inline bsv_status_t
bsv_read(bsv_in_t* in, void* buf, size_t size) {
	char* cbuf = buf;
	while (size > 0) {
		size_t bytes_read = in->read(in, cbuf, size);
		if (bytes_read == 0) { return BSV_IO_ERROR; }
		cbuf += bytes_read;
		size -= bytes_read;
	}

	return BSV_OK;
}

static inline bsv_status_t
bsv_write(bsv_out_t* out, const void* buf, size_t size) {
	const char* cbuf = buf;
	while (size > 0) {
		size_t bytes_written = out->write(out, cbuf, size);
		if (bytes_written == 0) { return BSV_IO_ERROR; }
		cbuf += bytes_written;
		size -= bytes_written;
	}

	return BSV_OK;
}

// Primitives

/*! Write an unsigned varint */
BSV_API bsv_status_t
bsv_write_uint(uint64_t x, bsv_out_t* out);

/*! Read an unsigned varint */
BSV_API bsv_status_t
bsv_read_uint(uint64_t* x, bsv_in_t* in);

/*! Write a signed varint */
BSV_API bsv_status_t
bsv_write_sint(int64_t x, bsv_out_t* out);

/*! Read a signed varint */
BSV_API bsv_status_t
bsv_read_sint(int64_t* x, bsv_in_t* in);

/*! Write a single precision float */
BSV_API bsv_status_t
bsv_write_f32(float f32, bsv_out_t* out);

/*! Read a single precision float */
BSV_API bsv_status_t
bsv_read_f32(float* f32, bsv_in_t* in);

/*! Write a double precision float */
BSV_API bsv_status_t
bsv_write_f64(double f64, bsv_out_t* out);

/*! Read a double precision float */
BSV_API bsv_status_t
bsv_read_f64(double* f64, bsv_in_t* in);

/**
 * @brief Read/write an unsigned varint
 */
BSV_API bsv_status_t
bsv_uint(bsv_ctx_t* ctx, uint64_t* value);

/**
 * @brief Read/write a signed varint
 */
BSV_API bsv_status_t
bsv_sint(bsv_ctx_t* ctx, int64_t* value);

/*! Read/write a single precision float */
BSV_API bsv_status_t
bsv_f32(bsv_ctx_t* ctx, float* value);

/*! Read/write a single double float */
BSV_API bsv_status_t
bsv_f64(bsv_ctx_t* ctx, double* value);

// Int type adapters

BSV_API bsv_status_t
bsv_i8(bsv_ctx_t* ctx, int8_t* i8);

BSV_API bsv_status_t
bsv_i16(bsv_ctx_t* ctx, int16_t* i16);

BSV_API bsv_status_t
bsv_i32(bsv_ctx_t* ctx, int32_t* i32);

BSV_API bsv_status_t
bsv_u8(bsv_ctx_t* ctx, uint8_t* u8);

BSV_API bsv_status_t
bsv_u16(bsv_ctx_t* ctx, uint16_t* u16);

BSV_API bsv_status_t
bsv_u32(bsv_ctx_t* ctx, uint32_t* u32);

BSV_API bsv_status_t
bsv_bool(bsv_ctx_t* ctx, bool* boolean);

BSV_API bsv_status_t
bsv_raw(bsv_ctx_t* ctx, void* data, size_t size);

BSV_API bsv_status_t
bsv_unknown(bsv_ctx_t* ctx, bsv_unkown_t* unknown);

/**
 * @brief Read/write a binary blob's header
 *
 * @param len Maximum size. Will be set to the actual size on read.
 */
BSV_API bsv_status_t
bsv_blob_header(bsv_ctx_t* ctx, bsv_len_t* len);

/*! @brief Read/write a binary blob's body */
BSV_API bsv_status_t
bsv_blob_body(bsv_ctx_t* ctx, char* buf);

// Versioned data

typedef struct {
	bsv_ctx_t* bsv;
	bsv_version_t previous_version;
	int counter;
} bsv_blk_ctx_t;

typedef struct {
	bsv_len_t previous_len;
	bsv_version_t previous_version;
	int counter;
} bsv_array_ctx_t;

BSV_API bsv_blk_ctx_t
bsv_begin_block(bsv_ctx_t* ctx, bsv_version_t version);

BSV_API bsv_status_t
bsv_end_block(bsv_ctx_t* ctx, bsv_blk_ctx_t* blk);

BSV_API bsv_status_t
bsv_begin_revision(bsv_ctx_t* ctx, bsv_version_t rev);

BSV_API bsv_status_t
bsv_end_revision(bsv_ctx_t* ctx);

BSV_API bsv_array_ctx_t
bsv_begin_array(bsv_ctx_t* ctx, bsv_len_t* length);

BSV_API bsv_status_t
bsv_end_array(bsv_ctx_t* ctx, bsv_array_ctx_t* array);

BSV_API bsv_in_t*
bsv__zero_in(void);

static inline void
bsv_trace(bsv_ctx_t* ctx, bsv_explain_type_t type, bsv_explain_scope_t scope) {
#ifdef BSV_REFLECTION
	if (ctx->explain_fn != NULL) {
		ctx->explain_info.type = type;
		ctx->explain_info.scope = scope;
		ctx->explain_fn(&ctx->explain_info, ctx->explain_ctx);
	}
#endif
}

static inline void
bsv_trace_begin(bsv_ctx_t* ctx, bsv_explain_type_t type) {
	bsv_trace(ctx, type, BSV_EXPLAIN_BEGIN_SCOPE);
}

static inline void
bsv_trace_end(bsv_ctx_t* ctx, bsv_explain_type_t type) {
	bsv_trace(ctx, type, BSV_EXPLAIN_END_SCOPE);
}

static inline void
bsv_set_explain_info(bsv_ctx_t* ctx, const char* file, int line, const char* function) {
#ifdef BSV_REFLECTION
	ctx->explain_info.file = file;
	ctx->explain_info.line = line;
	ctx->explain_info.function = function;
#endif
}

static inline bool
bsv_should_serialize_add(bsv_ctx_t* ctx) {
	return bsv_mode(ctx) == BSV_MODE_WRITE
		||
		ctx->current_blk_version >= ctx->current_revision;
}

static inline bool
bsv_should_serialize_rem(bsv_ctx_t* ctx, bsv_version_t version_removed) {
	return bsv_mode(ctx) == BSV_MODE_READ
		&&
		ctx->current_blk_version >= ctx->current_revision
		&&
		ctx->current_blk_version < version_removed;
}

BSV_CUSTOM_TYPES(BSV__DECLARE_SERIALIZER)

#ifdef BSV_STDIO

#include <stdio.h>

/*! stdio input stream */
typedef struct bsv_stdio_in_s {
	bsv_in_t bsv;
	FILE* file;
} bsv_stdio_in_t;

/*! stdio output stream */
typedef struct bsv_stdio_out_s {
	bsv_out_t bsv;
	FILE* file;
} bsv_stdio_out_t;

/*! Wrap a stdio FILE into an input stream */
BSV_API bsv_in_t*
bsv_stdio_init_in(bsv_stdio_in_t* bsv_stdio, FILE* file);

/*! Wrap a stdio FILE into an output stream */
BSV_API bsv_out_t*
bsv_stdio_init_out(bsv_stdio_out_t* bsv_stdio, FILE* file);

#endif

#ifdef BSV_MEM

/*! Memory input stream */
typedef struct bsv_mem_in_s {
	bsv_in_t bsv;
	char* cur;
	char* end;
} bsv_mem_in_t;

/*! Memory output stream */
typedef struct bsv_mem_out_s {
	bsv_out_t bsv;
	/*! Size of bsv_mem_out_t.mem */
	size_t len;
	size_t capacity;
	void* memctx;
	/**
	 * @brief The underlying memory.
	 *
	 * This should be manually freed.
	 */
	char* mem;
} bsv_mem_out_t;

/**
 * @brief Create a memory input stream
 * @param mem The backing memory.
 * @param size Size of the backing memory.
 * @return An input stream.
 */
BSV_API bsv_in_t*
bsv_mem_init_in(bsv_mem_in_t* bsv_mem, void* mem, size_t size);

/**
 * @brief Create a memory output stream
 * @param memctx The allocator context.
 * @return An output stream.
 * @see bsv_mem_out_t
 * @remarks
 *   bsv_mem_out_t.mem should be manually freed.
 */
BSV_API bsv_out_t*
bsv_mem_init_out(bsv_mem_out_t* bsv_mem, void* memctx);

#endif

#ifdef __cplusplus
}
#endif

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSV_IMPLEMENTATION)
#define BSV_IMPLEMENTATION
#endif

#ifdef BSV_IMPLEMENTATION

#include <string.h>

#define BSV_CHECK(CTX) BSV_CHECK_STATUS(bsv_status((CTX)))

bsv_status_t
bsv_write_uint(uint64_t x, bsv_out_t* out) {
    char buf[10];
    size_t n = 0;

	for (int i = 0; i < 10; ++i) {
		n += x >= 0x80;
		buf[i] = (char)(x | 0x80);
		x >>= 7;
	}

    buf[n] ^= 0x80;
	n += 1;

	return bsv_write(out, buf, n);
}

bsv_status_t
bsv_write_sint(int64_t x, bsv_out_t* out) {
    uint64_t ux = ((uint64_t)x << 1) ^ (uint64_t)(x >> 63);  // zigzag encode
    return bsv_write_uint(ux, out);
}

bsv_status_t
bsv_read_uint(uint64_t* x, bsv_in_t* in) {
	uint64_t b;
	char c;
	uint64_t tmp = 0;

	for (int i = 0; i < 10; ++i) {
		BSV_CHECK_STATUS(bsv_read(in, &c, 1));

		b = c;
		tmp |= (b & 0x7f) << (7 * i);
		if (b < 0x80) {
			*x = tmp;
			return BSV_OK;
		}
	}

	return BSV_MALFORMED;
}

bsv_status_t
bsv_read_sint(int64_t* x, bsv_in_t* in) {
    uint64_t ux;
	BSV_CHECK_STATUS(bsv_read_uint(&ux, in));
    *x = (int64_t)((ux >> 1) ^ (~(ux & 1) + 1));  // zigzag decode

    return BSV_OK;
}

bsv_status_t
bsv_write_f32(float f32, bsv_out_t* out) {
	uint32_t ivalue;
	memcpy(&ivalue, &f32, sizeof(f32));

	uint8_t buf[sizeof(ivalue)];
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		buf[i] = (uint8_t)(ivalue >> (i * 8));
	}

	return bsv_write(out, buf, sizeof(buf));
}

bsv_status_t
bsv_read_f32(float* f32, bsv_in_t* in) {
	uint32_t ivalue = 0;

	uint8_t buf[sizeof(ivalue)];
	BSV_CHECK_STATUS(bsv_read(in, buf, sizeof(buf)));
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		ivalue |= (uint32_t)buf[i] << (i * 8);
	}
	memcpy(f32, &ivalue, sizeof(ivalue));

	return BSV_OK;
}

bsv_status_t
bsv_write_f64(double f64, bsv_out_t* out) {
	uint64_t ivalue;
	memcpy(&ivalue, &f64, sizeof(f64));

	uint8_t buf[sizeof(ivalue)];
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		buf[i] = (uint8_t)(ivalue >> (i * 8));
	}

	return bsv_write(out, buf, sizeof(buf));
}

bsv_status_t
bsv_read_f64(double* f64, bsv_in_t* in) {
	uint64_t ivalue = 0;

	uint8_t buf[sizeof(ivalue)];
	BSV_CHECK_STATUS(bsv_read(in, buf, sizeof(buf)));
	for (size_t i = 0; i < sizeof(ivalue); ++i) {
		ivalue |= (uint64_t)buf[i] << (i * 8);
	}
	memcpy(f64, &ivalue, sizeof(ivalue));

	return BSV_OK;
}

static inline bsv_status_t
bsv_malformed(bsv_ctx_t* ctx) {
	return ctx->status = BSV_MALFORMED;
}

static bsv_status_t
bsv_raw_uint(bsv_ctx_t* ctx, uint64_t* value) {
	bsv_trace_begin(ctx, BSV_EXPLAIN_RAW);

	if (bsv_mode(ctx) == BSV_MODE_READ) {
		ctx->status = bsv_read_uint(value, ctx->in);
	} else {
		ctx->status = bsv_write_uint(*value, ctx->out);
	}

	bsv_trace_end(ctx, BSV_EXPLAIN_RAW);
	return ctx->status;
}

static bsv_status_t
bsv_raw_sint(bsv_ctx_t* ctx, int64_t* value) {
	bsv_trace_begin(ctx, BSV_EXPLAIN_RAW);

	if (bsv_mode(ctx) == BSV_MODE_READ) {
		ctx->status = bsv_read_sint(value, ctx->in);
	} else {
		ctx->status = bsv_write_sint(*value, ctx->out);
	}

	bsv_trace_end(ctx, BSV_EXPLAIN_RAW);
	return ctx->status;
}

bsv_status_t
bsv_uint(bsv_ctx_t* ctx, uint64_t* value) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	return bsv_raw_uint(ctx, value);
}

bsv_status_t
bsv_sint(bsv_ctx_t* ctx, int64_t* value) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	return bsv_raw_sint(ctx, value);
}

bsv_status_t
bsv_i8(bsv_ctx_t* ctx, int8_t* i8) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	int64_t i64 = *i8;
	BSV_CHECK_STATUS(bsv_raw_sint(ctx, &i64));

	if ((int64_t)INT8_MIN <= i64 && i64 <= (int64_t)INT8_MAX) {
		*i8 = (int8_t)i64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_i16(bsv_ctx_t* ctx, int16_t* i16) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	int64_t i64 = *i16;
	BSV_CHECK_STATUS(bsv_raw_sint(ctx, &i64));

	if ((int64_t)INT16_MIN <= i64 && i64 <= (int64_t)INT16_MAX) {
		*i16 = (int16_t)i64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_i32(bsv_ctx_t* ctx, int32_t* i32) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	int64_t i64 = *i32;
	BSV_CHECK_STATUS(bsv_raw_sint(ctx, &i64));

	if ((int64_t)INT32_MIN <= i64 && i64 <= (int64_t)INT32_MAX) {
		*i32 = (int32_t)i64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_u8(bsv_ctx_t* ctx, uint8_t* u8) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	uint64_t u64 = *u8;
	BSV_CHECK_STATUS(bsv_raw_uint(ctx, &u64));

	if (u64 <= (uint64_t)UINT8_MAX) {
		*u8 = (uint8_t)u64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_u16(bsv_ctx_t* ctx, uint16_t* u16) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	uint64_t u64 = *u16;
	BSV_CHECK_STATUS(bsv_raw_uint(ctx, &u64));

	if (u64 <= (uint64_t)UINT16_MAX) {
		*u16 = (uint16_t)u64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_u32(bsv_ctx_t* ctx, uint32_t* u32) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	uint64_t u64 = *u32;
	BSV_CHECK_STATUS(bsv_raw_uint(ctx, &u64));

	if (u64 <= (uint64_t)UINT32_MAX) {
		*u32 = (uint32_t)u64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_bool(bsv_ctx_t* ctx, bool* boolean) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);

	uint64_t u64 = *boolean;

	BSV_CHECK_STATUS(bsv_raw_uint(ctx, &u64));
	if (u64 <= 1) {
		*boolean = (bool)u64;
		return BSV_OK;
	} else {
		return bsv_malformed(ctx);
	}
}

bsv_status_t
bsv_f32(bsv_ctx_t* ctx, float* value) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);
	bsv_trace_begin(ctx, BSV_EXPLAIN_RAW);

	if (bsv_mode(ctx) == BSV_MODE_READ) {
		ctx->status = bsv_read_f32(value, ctx->in);
	} else {
		ctx->status = bsv_write_f32(*value, ctx->out);
	}

	bsv_trace_end(ctx, BSV_EXPLAIN_RAW);
	return ctx->status;
}

bsv_status_t
bsv_f64(bsv_ctx_t* ctx, double* value) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);
	bsv_trace_begin(ctx, BSV_EXPLAIN_RAW);

	if (bsv_mode(ctx) == BSV_MODE_READ) {
		ctx->status = bsv_read_f64(value, ctx->in);
	} else {
		ctx->status = bsv_write_f64(*value, ctx->out);
	}

	bsv_trace_end(ctx, BSV_EXPLAIN_RAW);
	return ctx->status;
}

bsv_status_t
bsv_blob_header(bsv_ctx_t* ctx, bsv_len_t* len) {
	BSV_CHECK_STATUS(bsv_uint(ctx, &ctx->blob_size));
	*len = ctx->blob_size;
	return BSV_OK;
}

bsv_status_t
bsv_blob_body(bsv_ctx_t* ctx, char* buf) {
	return bsv_raw(ctx, buf, ctx->blob_size);
}

bsv_status_t
bsv_raw(bsv_ctx_t* ctx, void* data, size_t size) {
	BSV_CHECK(ctx);
	BSV__SET_EXPLAIN_INFO(ctx);
	bsv_trace_begin(ctx, BSV_EXPLAIN_RAW);

	if (bsv_mode(ctx) == BSV_MODE_READ) {
		ctx->status = bsv_read(ctx->in, data, size);
	} else {
		ctx->status = bsv_write(ctx->out, data, size);
	}

	bsv_trace_end(ctx, BSV_EXPLAIN_RAW);
	return ctx->status;
}

bsv_blk_ctx_t
bsv_begin_block(bsv_ctx_t* ctx, bsv_version_t version) {
	ctx->explain_info.version = version;
	bsv_trace_begin(ctx, BSV_EXPLAIN_BLK);

	bsv_version_t prev_version = ctx->current_blk_version;
	ctx->current_blk_version = ctx->max_revision = version;

	if (ctx->explain_fn != NULL) {
		ctx->current_blk_version = version;
	} else if (ctx->array_len > 0) {
		if (ctx->array_version == (bsv_version_t)-1) {
			ctx->array_version = version;
			bsv_auto(ctx, &ctx->array_version);
		}

		ctx->current_blk_version = ctx->array_version;
	} else {
		bsv_auto(ctx, &ctx->current_blk_version);
	}

	return (bsv_blk_ctx_t){
		.bsv = ctx,
		.previous_version = prev_version
	};
}

bsv_status_t
bsv_end_block(bsv_ctx_t* ctx, bsv_blk_ctx_t* blk) {
	ctx->current_blk_version = blk->previous_version;

	bsv_trace_end(ctx, BSV_EXPLAIN_BLK);
	return bsv_status(ctx);
}

bsv_status_t
bsv_begin_revision(bsv_ctx_t* ctx, bsv_version_t revision) {
	ctx->explain_info.version = revision;
	bsv_trace_begin(ctx, BSV_EXPLAIN_REV);

	BSV_ASSERT(revision <= ctx->max_revision);
	ctx->current_revision = revision;
	return bsv_status(ctx);
}

bsv_status_t
bsv_end_revision(bsv_ctx_t* ctx) {
	bsv_trace_end(ctx, BSV_EXPLAIN_REV);
	return bsv_status(ctx);
}

bsv_array_ctx_t
bsv_begin_array(bsv_ctx_t* ctx, bsv_len_t* length) {
	bsv_trace_begin(ctx, BSV_EXPLAIN_ARRAY);

	bsv_len_t previous_len = ctx->array_len;
	bsv_version_t previous_version = ctx->array_version;

	if (ctx->explain_fn != NULL) {
		*length = 1;
	} else {
		bsv_auto(ctx, length);
	}
	ctx->array_len = *length;

	ctx->array_version = (bsv_version_t)-1;
	return (bsv_array_ctx_t){
		.previous_len = previous_len,
		.previous_version = previous_version,
	};
}

bsv_status_t
bsv_end_array(bsv_ctx_t* ctx, bsv_array_ctx_t* array) {
	ctx->array_len = array->previous_len;
	ctx->array_version = array->previous_version;

	bsv_trace_end(ctx, BSV_EXPLAIN_ARRAY);
	return bsv_status(ctx);
}

static inline size_t
bsv_zero_read(bsv_in_t* in, void* buf, size_t size) {
	(void)in;
	memset(buf, 0, size);
	return size;
}

bsv_in_t*
bsv__zero_in(void) {
	static bsv_in_t zero = { .read = bsv_zero_read };
	return &zero;
}

#ifdef BSV_STDIO

static inline size_t
bsv_stdio_read(bsv_in_t* in, void* buf, size_t size) {
	return fread(buf, size, 1, ((bsv_stdio_in_t*)in)->file) == 1 ? size : 0;
}

static inline size_t
bsv_stdio_write(bsv_out_t* out, const void* buf, size_t size) {
	return fwrite(buf, size, 1, ((bsv_stdio_out_t*)out)->file) == 1 ? size : 0;
}

bsv_in_t*
bsv_stdio_init_in(bsv_stdio_in_t* bsv_stdio, FILE* file) {
	*bsv_stdio = (bsv_stdio_in_t) {
		.bsv.read = bsv_stdio_read,
		.file = file,
	};
	return &bsv_stdio->bsv;
}

bsv_out_t*
bsv_stdio_init_out(bsv_stdio_out_t* bsv_stdio, FILE* file) {
	*bsv_stdio = (bsv_stdio_out_t) {
		.bsv.write = bsv_stdio_write,
		.file = file,
	};
	return &bsv_stdio->bsv;
}

#endif

#ifdef BSV_MEM

#ifndef BSV_REALLOC
#	ifdef BLIB_REALLOC
#		define BSV_REALLOC BLIB_REALLOC
#	else
#		define BSV_REALLOC(ptr, size, ctx) bsv_libc_realloc(ptr, size)
#		define BSV_USE_LIBC_REALLOC
#	endif
#endif

#ifdef BSV_USE_LIBC_REALLOC

#include <stdlib.h>

static inline void*
bsv_libc_realloc(void* ptr, size_t size) {
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

static inline size_t
bsv_mem_read(bsv_in_t* in, void* buf, size_t size) {
	bsv_mem_in_t* mem_in = (bsv_mem_in_t*)in;

	if (mem_in->cur + size <= mem_in->end) {
		memcpy(buf, mem_in->cur, size);
		mem_in->cur += size;
		return size;
	} else {
		return 0;
	}
}

static inline size_t
bsv_mem_write(bsv_out_t* out, const void* buf, size_t size) {
	bsv_mem_out_t* mem_out = (bsv_mem_out_t*)out;

	size_t required_capacity = mem_out->len + size;
	size_t capacity = mem_out->capacity;
	if (required_capacity > capacity) {
		size_t double_capacity = capacity * 2;
		size_t new_capacity = double_capacity > required_capacity
			? double_capacity
			: required_capacity;

		mem_out->mem = BSV_REALLOC(mem_out->mem, new_capacity, mem_out->memctx);
		if (mem_out->mem == NULL) { return 0; }

		mem_out->capacity = new_capacity;
	}

	memcpy(mem_out->mem + mem_out->len, buf, size);
	mem_out->len += size;
	return size;
}

bsv_in_t*
bsv_mem_init_in(bsv_mem_in_t* bsv_mem, void* mem, size_t size) {
	*bsv_mem = (bsv_mem_in_t){
		.bsv = {
			.read = bsv_mem_read,
		},
		.cur = mem,
		.end = (char*)mem + size,
	};
	return &bsv_mem->bsv;
}

bsv_out_t*
bsv_mem_init_out(bsv_mem_out_t* bsv_mem, void* memctx) {
	*bsv_mem = (bsv_mem_out_t){
		.bsv.write = bsv_mem_write,
		.len = 0,
		.capacity = 0,
		.mem = NULL,
		.memctx = memctx,
	};
	return &bsv_mem->bsv;
}

#endif

#endif
