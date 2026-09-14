#ifndef BSERIAL_H
#define BSERIAL_H

/**
 * @file
 *
 * @brief Serialization library.
 *
 * ## Motivation
 *
 * I need a serialization library with the following criteria:
 *
 * * Binary: No text parsing.
 * * No separate schema file: The serialization code is the schema.
 *   There is only a single function for both reading and writing.
 * * No separate object tree: There is only one phase: read/write data from the program's object directly to the input/output stream.
 *   This is different from many other libraries where a separate object tree (e.g: JSON) is created.
 *   Then, there is another phase to convert from that tree to internal representation.
 * * Schema evolution: The structure of objects can change.
 *   New keys can be added/removed/reordered.
 *   Existing keys can have their type changed.
 *   Old serialization files must still be compatible with newer versions.
 * * Fixed allocation: It should allocate a fixed amount of memory upfront.
 *   The memory complexity only depends on how deep the object tree can be, not how wide it is.
 *   There is always a bound check and invalid data is rejected.
 *
 * ## Design
 *
 * There are several layers:
 *
 * * Raw unstructed I/O: Functions to read/write varint and float in an endian-independent way.
 *   The functions generally have the forms:
 *
 *   * `bserial_read_<type>(<type> value, bserial_in_t* in)`: Read a value from a stream.
 *   * `bserial_write_<type>(<type> value, bserial_out_t* out)`: Write a value to a stream.
 * * I/O stream implementations: The above functions work with an abstract input or output stream.
 *   Out of the box, there are concrete implementations for stdio `FILE` and memory stream.
 * * Structured serialization: The meat of the library.
 *   Functions generally have the signature: `bserial_<type>(bserial_ctx_t* ctx, <type>* value)`.
 *   `ctx` is the current serialization context.
 *   Depending on how it was constructed, it will either read or write `value` to its stream.
 *   Thus, the same code can be used for both serialization and deserialization, forming a "schema".
 *
 *   `bserial_ctx_mem_size` is used to retrieve the fixed memory size needed for serialization/deserialization.
 *   Then `bserial_make_ctx` will be used to construct the context.
 *   Either an input stream or an output stream must be provided.
 *
 * ## Structured serialization
 *
 * The supported data types are:
 *
 * * signed/unsigned integers: They are stored as varint to save space since in practice, most integers are small.
 *   Internally, they are always converted into 64 bit.
 *   There are also convenient functions that does bound checking on read.
 *   e.g: A 8 bit unsgined integer will be checked to be in the range [0, 256].
 *   User code can do further range checking if needed.
 * * Floating points: Either single or double.
 *   IEEE 754 is assumed.
 * * Blob: An arbitrary binary blob of data.
 *   "String" is treated as blob in bserial.
 *   The library does not do any encoding validation.
 * * Symbol: Similar to strings but unique values are only written once.
 *   Subsequent occurences are referred to by id.
 *   For example, if the symbol `foo` has been written once as `[SYMBOL_DEF][foo]`, and then written again.
 *   `[SYMBOL_REF][0]` will be written to the stream instead.
 *   This helps to save space when certain sets of symbols are used repeatedly e.g: field names of a struct.
 * * Record: A collection of key-value pairs.
 *   The keys are always symbols.
 *   The values can have any types.
 *   The list of keys (the schema) is interned like a symbol.
 *   The first record with a given schema is written as `[RECORD_DEF][num_keys][keys...][values...]`.
 *   Later records with the same schema are written as `[RECORD_REF][id][values...]`.
 *   Repeated records such as array elements thus only pay for their values.
 * * Array: a collection of elements.
 *   Each element can have a different type.
 * * Table: a collection of elements
 *   Unlike array, each element must be a record.
 *   All records must have the same schema.
 *   The storage is optimized for storing repeated records of the same type.
 *
 * All data types are generally stored as `[tag][payload]`.
 * Variable length types like array are stored as `[tag][length][payload]`.
 * The type tag is needed to facilitate skipping over unknown data that is not described in the serialization code.
 *
 * ### Error handling
 *
 * `BSERIAL_CHECK_STATUS` should be used on all functions that returns `bserial_status_t`.
 * This ensures that the code returns as soon as possible when error is encountered.
 *
 * However, error is also "sticky".
 * As soon as a `bserial_ctx_t` encounters an error, all subsequent operations are noop.
 * This allows user code to trace the error with `bserial_trace`.
 *
 * TODO: Improve `bserial_trace`.
 *
 * ### Record
 *
 * Much of this library deals with reading and writing records in a backward compatible manner and allow for structural changes.
 * The serialization code for a struct must have the following form:
 *
 * ```c
 * bserial_status_t
 * serialize_my_struct(bserial_ctx_t* ctx, my_struct_t* my_struct) {
 *     while (bserial_record(ctx)) {
 *         // Serialize field "foo"
 *         if (bserial_key(ctx, "foo", sizeof("foo") - 1)) {
 *             BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &my_struct->foo));
 *         }
 *
 *         // Serialize field "bar"
 *         if (bserial_key(ctx, "bar", sizeof("bar") - 1)) {
 *             BSERIAL_CHECK_STATUS(bserial_f32(ctx, &my_struct->bar));
 *         }
 *
 *         // Other fields...
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * Or with the helper macros:
 *
 * ```c
 * bserial_status_t
 * serialize_my_struct(bserial_ctx_t* ctx, my_struct_t* my_struct) {
 *     BSERIAL_RECORD(ctx) {
 *         BSERIAL_KEY(ctx, foo) {
 *             BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &my_struct->foo));
 *         }
 *
 *         // Serialize field "bar"
 *         BSERIAL_KEY(ctx, bar) {
 *             BSERIAL_CHECK_STATUS(bserial_f32(ctx, &my_struct->bar));
 *         }
 *
 *         // Other fields...
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * The peculiar structure has to do with how the library handles record serialization.
 * Considering that it has to support the following:
 *
 * * New fields may be added.
 * * Existing fields may be removed.
 * * Existing fields may be reordered.
 * * Existing fields may have their types changed.
 *
 * The serialization code cannot be naively executed in order.
 *
 * The outer `while` loop allows the library to run mulitple passes through the structure.
 * This allows it to, for example, store the keys separately from the values.
 *
 * The inner `if` conditions allow the library to conditionally execute the value serialization code.
 * This allows it to reorder and toggle the execution of each field.
 * This can arguably be done with a function table where each key is a field name and each value is a serialization function.
 * However, it is much less ergonomic to do in C.
 * Even in C++, there are concerns about where the captures and the function table itself is allocated.
 * This violates the library's goal of fixed allocation.
 * In the common cases (code and data perfectly matches), the field clauses will most likely be executed in order anyway.
 * Thus there should not be much overhead.
 * Even with some omitted fields (due to newly introduced fields in code), given that repeated records usually share the same schema, the branches should be perfectly predictable.
 * e.g: "foo" is always executed and "bar" is always skipped.
 *
 * #### Compatible type change
 *
 * The above structure allows us to deal with compatible structural changes:
 *
 * * Field reordering: The outer `while` loop allows field listing in any arbitrary order.
 * * Addition of new fields: The inner `if` conditions allow new fields in code but not in data to be toggled off.
 * * Removal of existing fields: Due to the way data is written, unrecogized fields will be automatically skipped over and not read.
 *   Internally, the call to `bserial_record` will skip to the closest recognized field in the data stream.
 *   As an optimization, after a field is read, the same skipping is done again.
 *   In the most common case where the data was written and then read by the same piece of code, there would be only two passes through the while loop:
 *
 *   * The first pass is for schema discovery.
 *     Each `bserial_key` call in the code is mapped to a corresponding field in the data.
 *     They all return false to skip the value serialization code.
 *   * The second pass is for serialization.
 *     Since data was written in order, the first `bserial_key` call would return true.
 *     After the value is read, the library recognizes that the second `bserial_key` call is also a match in order and return true again, and so on...
 *
 *   Thus there would be only two iterations through the `while`loop.
 *   Records with the same set of keys share one schema in the stream.
 *   The keys are only written for the first one, later ones only refer to the schema by id.
 *
 *   The loop would only have to run more than twice when the program has to read serialized data from a previous version where fields have a different order.
 *   This can be somewhat mitigated by only adding code for new fields at the end of the serialization function.
 *
 * #### Incompatible type change
 *
 * Suppose that we have the following struct:
 *
 * ```c
 * typedef struct {
 *     int foo;
 * } my_struct_t;
 * ```
 *
 * And later we want to change the type into:
 *
 * ```c
 * typedef struct {
 *     float foo;
 * } my_struct_t;
 * ```
 *
 * This would be an incompatible change that the library cannot deal with out of the box.
 * However, the following idiom can be used:
 *
 * ```c
 * bserial_status_t
 * serialize_my_struct(bserial_ctx_t* ctx, my_struct_t* my_struct) {
 *     BSERIAL_RECORD(ctx) {
 *         // Compatibility code path for reading from older versions
 *         if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
 *             // Read the old field as if it is the old version
 *             BSERIAL_KEY(ctx, foo) {
 *                 int foo;  // We know that we are reading so
 *                           // `foo = my_struct->foo` is unnecessary.
 *                 BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &foo));
 *                 // Convert to the new version
 *                 my_struct->foo = (float)foo;
 *             }
 *         }
 *
 *         // For compatibility, when an incompatible change is made, a different
 *         // field name must be used in the serialized data.
 *         BSERIAL_KEY(ctx, foo_v2) {
 *             // The internal representation can use any field name, thus `foo` is
 *             // still legal.
 *             BSERIAL_CHECK_STATUS(bserial_float(ctx, &my_struct->foo));
 *         }
 *     }
 * }
 * ```
 *
 * Versioning can thus, be done per field instead of per record.
 * This should make backward-compatible schema change relatively painless.
 *
 * ### Enum
 *
 * An enum is stored as a symbol: the name of the active variant.
 * Like record keys, the names are declared in the serialization code and the
 * library matches the stream against them.
 * No lookup table is stored in the stream and the numeric values of the C enum
 * never leave the program, so they can be renumbered or reordered freely.
 *
 * ```c
 * typedef enum {
 *     SHAPE_CIRCLE,
 *     SHAPE_SQUARE,
 *     SHAPE_TRIANGLE,
 * } shape_kind_t;
 *
 * bserial_status_t
 * serialize_shape_kind(bserial_ctx_t* ctx, shape_kind_t* kind) {
 *     BSERIAL_ENUM(ctx, kind) {
 *         BSERIAL_VARIANT(ctx, SHAPE_CIRCLE);
 *         BSERIAL_VARIANT(ctx, SHAPE_SQUARE);
 *         BSERIAL_VARIANT(ctx, SHAPE_TRIANGLE);
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * `BSERIAL_ENUM` takes a pointer to an integer or enum of any type.
 * When writing, the variant whose value matches the pointee writes its name.
 * When reading, the variant whose name matches the stream stores its value into the pointee.
 * A value with no name or a name with no variant is an error.
 *
 * Renaming a variant is handled like renaming a record field:
 * keep the old name as a read-only alias.
 *
 * ```c
 * BSERIAL_ENUM(ctx, kind) {
 *     if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
 *         // Data written before the rename
 *         bserial_variant(ctx, "SHAPE_BOX", sizeof("SHAPE_BOX") - 1, SHAPE_SQUARE);
 *     }
 *     BSERIAL_VARIANT(ctx, SHAPE_CIRCLE);
 *     BSERIAL_VARIANT(ctx, SHAPE_SQUARE);
 * }
 * ```
 *
 * ### Variable length types
 *
 * Beside the initial fixed-size memory buffer passed to `bserial_make_ctx`, the library does not allocate any more memory.
 * For variable-length data types such as array, user code has to make their own decision.
 * In general, fixed allocation is encouraged and it is always important to enforce a hard limit on length.
 * However, when dynamic size is needed something like this can be used:
 *
 * ```c
 * bserial_status_t
 * serialize_my_array(bserial_ctx_t* ctx, my_array_t* array) {  // The program's array type
 *     int len = my_array_len(array);  // Put length into a variable
 *     BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));  // Serialize length
 *     if (len > MY_ARRAY_MAX_LEN) { return BSERIAL_MALFORMED; }  // Limit check
 *     my_array_resize(array, len);  // Dynamically resize the array to fit
 *
 *     // Serialize body
 *     for (int i = 0; i < my_array_len(array); ++i) {
 *         BSERIAL_CHECK_STATUS(serialize_element(ctx, my_array_element_at(array, i)));
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * In the write path, `my_array_len_resize` should be a noop.
 * In the read path, `my_array_len_resize` will adjust the array to have the appropriate size.
 *
 * In general, variable length type should follow the pattern of:
 *
 * 1. Assign length to a variable.
 * 2. Read/write length with `bserial_array`.
 * 3. Limit check
 * 4. If using dynamic memory, resize storage to fit length.
 * 5. Loop through elements and serialize them.
 *
 * Lengths, like `bserial_any_int`, accept a pointer to any integer type.
 * They are widened to 64 bits on the wire.
 * A negative length on write or a length that does not fit in the variable on read is an error.
 * The `_u64` variants such as `bserial_array_u64` are the underlying functions.
 *
 * ### Table
 *
 * A table is a special case of array where all records have the same schema.
 * The schema is written with the first row and the remaining rows only contain their values, saving 2 bytes per row.
 *
 * ```c
 * bserial_status_t
 * serialize_my_table(bserial_ctx_t* ctx, my_table_t* table) {
 *     int len = my_table_len(table);  // Put length into a variable
 *     BSERIAL_CHECK_STATUS(bserial_table(ctx, &len));  // Serialize length
 *     if (len > MY_TABLE_MAX_LEN) { return BSERIAL_MALFORMED; }  // Limit check
 *     my_table_resize(table, len);  // Dynamically resize the table to fit
 *
 *     // Serialize rows
 *     for (int i = 0; i < my_table_len(table); ++i) {
 *         BSERIAL_CHECK_STATUS(serialize_my_struct(ctx, my_table_row_at(table, i)));
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * Every row must be a record and all rows must have the same set of keys.
 * Writing a row with a different set of keys is an error.
 *
 * ### String encoding validation
 *
 * This is considered to be out of scope.
 * However, it is not difficult to create a helper in user code:
 *
 * ```c
 * bserial_status_t
 * serialize_utf8_str(bserial_ctx_t* ctx, my_string_t* str) {  // The program's string type
 *     int len = my_string_len(str);  // Put length into a variable
 *     BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));  // Serialize length
 *     if (len > MY_STRING_MAX_LEN) { return BSERIAL_MALFORMED; }  // Limit check
 *     my_string_resize(str, len);  // Dynamically resize the string
 *
 *     BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, my_string_cstr(str));  // Serialize string body
 *     // Assuming that all internal strings are valid, we only have to validate on read
 *     if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
 *         if (!validate_utf8(str)) { return BSERIAL_MALFORMED; }
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * ### Variant/tagged union types
 *
 * There is no built-in support for these types.
 * However, they could be emulated with an array where:
 *
 * * The first element is the type tag.
 * * The second element is the payload.
 *
 * ```c
 * bserial_status_t
 * serialize_variant(bserial_ctx_t* ctx, my_variant_t* variant) {
 *     int len = 2;  // The size is always 2
 *     BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
 *     if (len != 2) { return BSERIAL_MALFORMED; }
 *
 *     BSERIAL_CHECK_STATUS(serialize_my_variant_type(ctx, &variant->type));  // An enum
 *     switch (variant->type) {
 *         case MY_VARIANT_TYPE_1:
 *             return serialize_variant_type_1(ctx, &variant->payload.type1);
 *         case MY_VARIANT_TYPE_2:
 *             return serialize_variant_type_2(ctx, &variant->payload.type2);
 *         // ...
 *         default: // Always have a default case to guard against invalid data
 *             return BSERIAL_MALFORMED;
 *     }
 * }
 * ```
 *
 * Record is not appropriate for this usecase as fields in records are unordered.
 * There is no guarantee that the type field will be read before the payload fields.
 *
 * ### Associative map
 *
 * Record is optimized for serializing a fixed set of keys and it is not suitable for associative map types such as hashtable.
 * Instead, an array of records should be used:
 *
 * ```c
 * bserial_status_t
 * serialize_hashmap(bserial_ctx_t* ctx, my_hashmap_t* hashmap) {
 *     size_t len = my_hashmap_len(hashmap);
 *     BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
 *
 *     if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
 *         // Optionally, reserve
 *         my_hashmap_reserve(hashmap, len);
 *         // Typically, on read, the input map should be empty but we can also clear it here
 *         my_hashmap_clear(hashmap)
 *
 *         for (uint64_t i = 0; i < len; ++i) {
 *             my_hashmap_entry_t entry;
 *             BSERIAL_CHECK_STATUS(serialize_hashmap_entry(ctx, &entry));
 *             my_hashmap_put(hashmap, entry.key, entry.value);
 *         }
 *     } else {
 *         // Assuming this is the iteration API
 *         for (
 *             my_hashmap_entry_t* itr = my_hashmap_begin(hashmap);
 *             itr != my_hashmap_end(hashmap);
 *             itr = my_hasmap_next(itr)
 *         ) {
 *             BSERIAL_CHECK_STATUS(serialize_hashmap_entry(ctx, itr));
 *         }
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * Where `serialize_hashmap_entry` defines a record:
 *
 * ```c
 * bserial_status_t
 * serialize_hashmap_entry(bserial_ctx_t* ctx, my_hashmap_entry_t* entry) {
 *     BSERIAL_RECORD(ctx) {
 *         BSERIAL_KEY(ctx, key) {
 *             BSERIAL_CHECK_STATUS(serialize_key_type(ctx, &entry->key));
 *         }
 *
 *         BSERIAL_KEY(ctx, value) {
 *             BSERIAL_CHECK_STATUS(serialize_value_type(ctx, &entry->value));
 *         }
 *     }
 *
 *     return bserial_status(ctx);
 * }
 * ```
 *
 * The nature of hashmap is such that the write path and the read path have totally different code due to hashing.
 * We cannot quite use the same code for both reading and writing.
 * However, the entry serialization code can still be shared.
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
		bserial_status_t bserial__status = OP; \
		if (bserial__status != BSERIAL_OK) { return bserial__status; } \
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
	 * @brief Maximum number of distinct record schemas.
	 *
	 * Records with the same set of keys share a schema.
	 *
	 * @see bserial_record
	 */
	uint32_t max_num_schemas;
	/**
	 * @brief Maximum nested depth.
	 * @see bserial_record
	 * @see bserial_array
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

/*! Read exactly @a size bytes from an input stream */
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

/*! Skip exactly @a size bytes from an input stream */
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

/*! Write exactly @a size bytes to an output stream */
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
 * @param config The configuration.
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

/*! @brief Read/write a boolean */
BSERIAL_API bserial_status_t
bserial_bool(bserial_ctx_t* ctx, bool* boolean);

// Generic integer adapters

/**
 * @brief Description of an integer type.
 *
 * @see BSERIAL_INT_TYPE
 */
typedef struct {
	uint8_t size;
	bool is_signed;
} bserial_int_type_t;

/**
 * @brief Describe the integer type pointed to by @a ptr.
 *
 * @a ptr must point to an integer type.
 * Any other pointer type is a compile error.
 */
#define BSERIAL_INT_TYPE(ptr) \
	((bserial_int_type_t){ \
		/* % is only defined for integer types */ \
		.size = (uint8_t)(sizeof(*(ptr)) + 0 * sizeof(*(ptr) % 1)), \
		.is_signed = _Generic((ptr), \
			bool*: false, \
			char*: CHAR_MIN < 0, \
			signed char*: true, \
			unsigned char*: false, \
			short*: true, \
			unsigned short*: false, \
			int*: true, \
			unsigned int*: false, \
			long*: true, \
			unsigned long*: false, \
			long long*: true, \
			unsigned long long*: false, \
			default: true \
		), \
	})

/**
 * @brief Read/write an integer of any type.
 *
 * The value is widened to 64 bits for serialization.
 * On read, a value that does not fit in the type is an error.
 *
 * @see bserial_any_int
 */
BSERIAL_API bserial_status_t
bserial_typed_int(bserial_ctx_t* ctx, void* value, bserial_int_type_t type);

/**
 * @brief Read/write an integer of any type.
 *
 * @param ctx The serialization context.
 * @param integer Pointer to an integer of any fundamental type or a typedef of one.
 */
#define bserial_any_int(ctx, integer) \
	bserial_typed_int(ctx, (integer), BSERIAL_INT_TYPE(integer))

/*! @brief A length operation on 64 bits. */
typedef bserial_status_t (*bserial_len_fn_t)(bserial_ctx_t* ctx, uint64_t* len);

/**
 * @brief Apply a length operation to a length of any integer type.
 *
 * A negative length on write or a length that does not fit in the type on
 * read is an error.
 *
 * @see bserial_array
 * @see bserial_blob_header
 */
BSERIAL_API bserial_status_t
bserial_typed_len(bserial_ctx_t* ctx, bserial_len_fn_t fn, void* len, bserial_int_type_t type);

/**
 * @brief Read/write a binary blob
 *
 * Instead of combining into one step, one can also use: @ref bserial_blob_header
 * and @ref bserial_blob_body.
 *
 * @param ctx The serialization context.
 * @param buf The buffer to read/write.
 * @param len Size of the buffer. Will be set to the actual size.
 *
 * @see bserial_blob
 */
BSERIAL_API bserial_status_t
bserial_blob_u64(bserial_ctx_t* ctx, char* buf, uint64_t* len);

/**
 * @brief Read/write a binary blob
 *
 * Same as @ref bserial_blob_u64 but @a len can be a pointer to any integer type.
 *
 * @param ctx The serialization context.
 * @param buf The buffer to read/write.
 * @param len Pointer to the size of the buffer. Will be set to the actual size.
 */
#define bserial_blob(ctx, buf, len) \
	bserial_typed_blob(ctx, buf, (len), BSERIAL_INT_TYPE(len))

/*! @see bserial_blob */
BSERIAL_API bserial_status_t
bserial_typed_blob(bserial_ctx_t* ctx, char* buf, void* len, bserial_int_type_t type);

/**
 * @brief Read/write a binary blob's header
 *
 * @param ctx The serialization context.
 * @param len Maximum size. Will be set to the actual size on read.
 *
 * @see bserial_blob_header
 */
BSERIAL_API bserial_status_t
bserial_blob_header_u64(bserial_ctx_t* ctx, uint64_t* len);

/**
 * @brief Read/write a binary blob's header
 *
 * Same as @ref bserial_blob_header_u64 but @a len can be a pointer to any integer type.
 */
#define bserial_blob_header(ctx, len) \
	bserial_typed_len(ctx, bserial_blob_header_u64, (len), BSERIAL_INT_TYPE(len))

/*! @brief Read/write a binary blob's body */
BSERIAL_API bserial_status_t
bserial_blob_body(bserial_ctx_t* ctx, char* buf);

/**
 * @brief Read/write a symbol.
 *
 * @param ctx The serialization context.
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
bserial_array_u64(bserial_ctx_t* ctx, uint64_t* len);

/**
 * @brief Read/write an array.
 *
 * Same as @ref bserial_array_u64 but @a len can be a pointer to any integer type.
 */
#define bserial_array(ctx, len) \
	bserial_typed_len(ctx, bserial_array_u64, (len), BSERIAL_INT_TYPE(len))

/**
 * @brief Read/write a table.
 *
 * A table is an array of records with the same schema.
 * After this call @a len records are expected.
 * Every row must have the same set of keys.
 *
 * @see bserial_record
 */
BSERIAL_API bserial_status_t
bserial_table_u64(bserial_ctx_t* ctx, uint64_t* len);

/**
 * @brief Read/write a table.
 *
 * Same as @ref bserial_table_u64 but @a len can be a pointer to any integer type.
 */
#define bserial_table(ctx, len) \
	bserial_typed_len(ctx, bserial_table_u64, (len), BSERIAL_INT_TYPE(len))

/**
 * @brief Read/write a record.
 *
 * A record is a collection of key/value pairs.
 *
 * The library needs to make several passes over the structure of the record.
 * This should always be called as the condition of a while loop:
 * `while (bserial_record(ctx)) {`.
 * Therefore, the macro @ref BSERIAL_RECORD should be used.
 *
 * A nested record must be serialized inside a @ref bserial_key block, like
 * any other value.
 *
 * @param ctx The serialization context.
 *
 * @see bserial_key
 */
BSERIAL_API bool
bserial_record(bserial_ctx_t* ctx);

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
 * @param ctx The serialization context.
 * @param name Name of the field being serialized.
 * @param len Length of the field name being serialized.
 */
BSERIAL_API bool
bserial_key(bserial_ctx_t* ctx, const char* name, uint64_t len);

/**
 * @brief Read/write a record.
 *
 * @param ctx The serialization context.
 */
#define BSERIAL_RECORD(ctx) while (bserial_record(ctx))

/**
 * Read/write a key in a record
 *
 * @param ctx The serialization context.
 * @param name Literal name of a record field, without any quote.
 *   e.g: `foo` and **not** `"foo"`.
 */
#define BSERIAL_KEY(ctx, name) if (bserial_key(ctx, #name, sizeof(#name) - 1))

/**
 * @brief Read/write an enum.
 *
 * An enum is stored as a symbol: the name of the active variant.
 *
 * This should always be called as the condition of a while loop:
 * `while (bserial_typed_enum(ctx, value, type)) {`.
 * Therefore, the macro @ref BSERIAL_ENUM should be used.
 * The body of the loop must only contain @ref bserial_variant calls.
 *
 * When writing, the variant whose value matches the pointed-to integer
 * writes its name.
 * When reading, the variant whose name matches the stream stores its value
 * into the pointed-to integer.
 * It is an error if no variant matches or if the value does not fit in the
 * integer type.
 *
 * @param ctx The serialization context.
 * @param value Pointer to the enum value, an integer of any type.
 * @param type Description of the pointed-to type.
 *
 * @see bserial_variant
 * @see BSERIAL_INT_TYPE
 */
BSERIAL_API bool
bserial_typed_enum(bserial_ctx_t* ctx, void* value, bserial_int_type_t type);

/**
 * @brief Declare a variant of an enum.
 *
 * This can only be called within a @ref BSERIAL_ENUM block.
 *
 * @param ctx The serialization context.
 * @param name Name of the variant.
 * @param len Length of the name.
 * @param value Value of the variant.
 *
 * @see bserial_typed_enum
 */
BSERIAL_API bserial_status_t
bserial_variant(bserial_ctx_t* ctx, const char* name, uint64_t len, int value);

/**
 * @brief Read/write an enum.
 *
 * Same as @ref bserial_typed_enum but the type is inferred from @a value.
 *
 * @param ctx The serialization context.
 * @param value Pointer to the enum value, an integer or enum of any type.
 *   It is evaluated once per loop iteration and must not have side effects.
 *
 * @see bserial_typed_enum
 * @see BSERIAL_INT_TYPE
 */
#define BSERIAL_ENUM(ctx, value) \
	while (bserial_typed_enum(ctx, (value), BSERIAL_INT_TYPE(value)))

/**
 * Declare a variant in an enum
 *
 * @param ctx The serialization context.
 * @param name Literal name of an enum variant, without any quote.
 *   e.g: `FOO` and **not** `"FOO"`.
 *   It is also used as the value.
 */
#define BSERIAL_VARIANT(ctx, name) bserial_variant(ctx, #name, sizeof(#name) - 1, name)

/*! Trace the error context during serialization */
BSERIAL_API void
bserial_trace(bserial_ctx_t* ctx, bserial_tracer_t tracer, void* userdata);

#ifdef BSERIAL_STDIO

#include <stdio.h>

/*! stdio input stream */
typedef struct bserial_stdio_in_s {
	/// @cond INTERNAL
	bserial_in_t bserial;
	FILE* file;
	/// @endcond
} bserial_stdio_in_t;

/*! stdio output stream */
typedef struct bserial_stdio_out_s {
	/// @cond INTERNAL
	bserial_out_t bserial;
	FILE* file;
	/// @endcond
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
	/// @cond INTERNAL
	bserial_in_t bserial;
	char* cur;
	char* end;
	/// @endcond
} bserial_mem_in_t;

/*! Memory output stream */
typedef struct bserial_mem_out_s {
	/// @cond INTERNAL
	bserial_out_t bserial;
	/// @endcond
	/*! Size of bserial_mem_out_t.mem */
	size_t len;
	/// @cond INTERNAL
	size_t capacity;
	void* memctx;
	/// @endcond
	/**
	 * @brief The underlying memory.
	 *
	 * This should be manually freed.
	 */
	char* mem;
} bserial_mem_out_t;

/**
 * @brief Create a memory input stream
 * @param bserial_mem The stream to initialize.
 * @param mem The backing memory.
 * @param size Size of the backing memory.
 * @return An input stream.
 */
BSERIAL_API bserial_in_t*
bserial_mem_init_in(bserial_mem_in_t* bserial_mem, void* mem, size_t size);

/**
 * @brief Create a memory output stream
 * @param bserial_mem The stream to initialize.
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
	BSERIAL_RECORD_DEF   =  8,
	BSERIAL_RECORD_REF   =  9,
	BSERIAL_ARRAY        = 10,
	BSERIAL_TABLE        = 11,
} bserial_marker_t;

typedef enum {
	BSERIAL_SCOPE_ROOT,
	BSERIAL_SCOPE_BLOB,
	BSERIAL_SCOPE_ARRAY,
	BSERIAL_SCOPE_TABLE,
	BSERIAL_SCOPE_RECORD,
	BSERIAL_SCOPE_ENUM,
} bserial_scope_type_t;

typedef enum {
	BSERIAL_OP_NUMERIC,
	BSERIAL_OP_BLOB,
	BSERIAL_OP_SYMBOL,
	BSERIAL_OP_ARRAY,
	BSERIAL_OP_TABLE,
	BSERIAL_OP_RECORD,
	BSERIAL_OP_ENUM,
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

// An interned list of keys
typedef struct {
	bserial_symbol_t* fields;
	uint32_t num_fields;
} bserial_schema_t;

typedef struct {
	bserial_scope_type_t type;

	uint64_t iterator;
	uint64_t len;

	bserial_record_mode_t record_mode;
	bserial_record_mapping_t* record_schema;
	bserial_record_mapping_t* prev_schema_pool;
	// Set when bserial_key returns true and cleared by the value op that
	// follows. Used to tell a nested record apart from the loop head of the
	// current record.
	bool value_pending;

	// The schema shared by all rows of a table, set by the first row
	bserial_schema_t* table_schema;

	void* enum_value;
	bserial_int_type_t enum_type;
	const char* enum_symbol;
	uint64_t enum_symbol_len;
	bool enum_matched;
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

	bserial_schema_t* schemas;
	uint32_t num_schemas;
	int32_t* schema_index;
	int32_t schema_exp;
	bserial_symbol_t* schema_fields;

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

	ptrdiff_t schemas = mem_layout_reserve(
		&layout,
		sizeof(bserial_schema_t) * config.max_num_schemas,
		_Alignof(bserial_schema_t)
	);

	int32_t schema_exp = 2;
	while (((int32_t)1 << schema_exp) < (int32_t)(config.max_num_schemas * 2)) {
		++schema_exp;
	}
	int32_t schema_index_len = ((int32_t)1 << schema_exp);
	ptrdiff_t schema_index = mem_layout_reserve(
		&layout,
		sizeof(int32_t) * schema_index_len,
		_Alignof(int32_t)
	);

	ptrdiff_t schema_fields = mem_layout_reserve(
		&layout,
		sizeof(bserial_symbol_t) * config.max_num_schemas * config.max_record_fields,
		_Alignof(bserial_symbol_t)
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

		ctx->schemas = mem_layout_locate(mem, schemas);
		ctx->schema_index = mem_layout_locate(mem, schema_index);
		ctx->schema_exp = schema_exp;
		memset(ctx->schema_index, 0, sizeof(*ctx->schema_index) * schema_index_len);
		ctx->schema_fields = mem_layout_locate(mem, schema_fields);

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

	if (type == BSERIAL_SCOPE_RECORD) {
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

	// An enum only contains the symbol of its active variant
	if (
		scope_type == BSERIAL_SCOPE_ENUM
		&& op != BSERIAL_OP_SYMBOL
	) {
		return bserial_malformed(ctx);
	}

	// Count the number of elements
	if (scope_type == BSERIAL_SCOPE_ARRAY) {
		++scope->iterator;
	}

	// A table only contains records
	if (scope_type == BSERIAL_SCOPE_TABLE) {
		if (op != BSERIAL_OP_RECORD) {
			return bserial_malformed(ctx);
		}
		++scope->iterator;
	}

	// The value following a key is being consumed
	if (scope_type == BSERIAL_SCOPE_RECORD) {
		scope->value_pending = false;
	}

	if (op == BSERIAL_OP_BLOB) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_BLOB));
	} else if (op == BSERIAL_OP_ARRAY) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_ARRAY));
	} else if (op == BSERIAL_OP_TABLE) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_TABLE));
	} else if (op == BSERIAL_OP_RECORD) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_RECORD));
	} else if (op == BSERIAL_OP_ENUM) {
		BSERIAL_CHECK_STATUS(bserial_push_scope(ctx, BSERIAL_SCOPE_ENUM));
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
		|| (ctx->scope->type == BSERIAL_SCOPE_ENUM && op == BSERIAL_OP_ENUM)
	) {
		BSERIAL_CHECK_STATUS(bserial_pop_scope(ctx));
	}

	// Auto pop when enough ops are executed.
	// Array and table do not have an "end" function call and the number of
	// elements is automatically tracked through bserial_end_op.
	while (
		(ctx->scope->type == BSERIAL_SCOPE_ARRAY || ctx->scope->type == BSERIAL_SCOPE_TABLE)
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

static inline bool
bserial_load_sint(const void* value, uint8_t size, int64_t* out) {
	switch (size) {
		case 1: { int8_t  v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		case 2: { int16_t v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		case 4: { int32_t v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		case 8: { int64_t v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		default: return false;
	}
}

static inline bool
bserial_load_uint(const void* value, uint8_t size, uint64_t* out) {
	switch (size) {
		case 1: { uint8_t  v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		case 2: { uint16_t v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		case 4: { uint32_t v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		case 8: { uint64_t v; memcpy(&v, value, sizeof(v)); *out = v; return true; }
		default: return false;
	}
}

// Store with range check
static inline bool
bserial_store_sint(void* value, uint8_t size, int64_t in) {
	switch (size) {
		case 1: {
			if (in < INT8_MIN || in > INT8_MAX) { return false; }
			int8_t v = (int8_t)in; memcpy(value, &v, sizeof(v)); return true;
		}
		case 2: {
			if (in < INT16_MIN || in > INT16_MAX) { return false; }
			int16_t v = (int16_t)in; memcpy(value, &v, sizeof(v)); return true;
		}
		case 4: {
			if (in < INT32_MIN || in > INT32_MAX) { return false; }
			int32_t v = (int32_t)in; memcpy(value, &v, sizeof(v)); return true;
		}
		case 8: { memcpy(value, &in, sizeof(in)); return true; }
		default: return false;
	}
}

// Store with range check
static inline bool
bserial_store_uint(void* value, uint8_t size, uint64_t in) {
	switch (size) {
		case 1: {
			if (in > UINT8_MAX) { return false; }
			uint8_t v = (uint8_t)in; memcpy(value, &v, sizeof(v)); return true;
		}
		case 2: {
			if (in > UINT16_MAX) { return false; }
			uint16_t v = (uint16_t)in; memcpy(value, &v, sizeof(v)); return true;
		}
		case 4: {
			if (in > UINT32_MAX) { return false; }
			uint32_t v = (uint32_t)in; memcpy(value, &v, sizeof(v)); return true;
		}
		case 8: { memcpy(value, &in, sizeof(in)); return true; }
		default: return false;
	}
}

bserial_status_t
bserial_typed_int(bserial_ctx_t* ctx, void* value, bserial_int_type_t type) {
	BSERIAL_CHECK_STATUS(ctx->status);

	if (type.is_signed) {
		int64_t wide;
		if (!bserial_load_sint(value, type.size, &wide)) { return bserial_malformed(ctx); }
		BSERIAL_CHECK_STATUS(bserial_sint(ctx, &wide));
		if (!bserial_store_sint(value, type.size, wide)) { return bserial_malformed(ctx); }
	} else {
		uint64_t wide;
		if (!bserial_load_uint(value, type.size, &wide)) { return bserial_malformed(ctx); }
		BSERIAL_CHECK_STATUS(bserial_uint(ctx, &wide));
		if (!bserial_store_uint(value, type.size, wide)) { return bserial_malformed(ctx); }
	}

	return BSERIAL_OK;
}

// A length is unsigned on the wire but may live in a signed variable
static inline bserial_status_t
bserial_load_len(bserial_ctx_t* ctx, const void* len, bserial_int_type_t type, uint64_t* out) {
	if (type.is_signed) {
		int64_t wide;
		if (!bserial_load_sint(len, type.size, &wide)) { return bserial_malformed(ctx); }
		if (wide < 0) { return bserial_malformed(ctx); }
		*out = (uint64_t)wide;
	} else {
		if (!bserial_load_uint(len, type.size, out)) { return bserial_malformed(ctx); }
	}

	return BSERIAL_OK;
}

static inline bserial_status_t
bserial_store_len(bserial_ctx_t* ctx, void* len, bserial_int_type_t type, uint64_t in) {
	if (type.is_signed) {
		if (in > INT64_MAX) { return bserial_malformed(ctx); }
		if (!bserial_store_sint(len, type.size, (int64_t)in)) { return bserial_malformed(ctx); }
	} else {
		if (!bserial_store_uint(len, type.size, in)) { return bserial_malformed(ctx); }
	}

	return BSERIAL_OK;
}

bserial_status_t
bserial_typed_len(bserial_ctx_t* ctx, bserial_len_fn_t fn, void* len, bserial_int_type_t type) {
	BSERIAL_CHECK_STATUS(ctx->status);

	uint64_t wide;
	BSERIAL_CHECK_STATUS(bserial_load_len(ctx, len, type, &wide));
	BSERIAL_CHECK_STATUS(fn(ctx, &wide));
	return bserial_store_len(ctx, len, type, wide);
}

bserial_status_t
bserial_typed_blob(bserial_ctx_t* ctx, char* buf, void* len, bserial_int_type_t type) {
	BSERIAL_CHECK_STATUS(ctx->status);

	uint64_t wide;
	BSERIAL_CHECK_STATUS(bserial_load_len(ctx, len, type, &wide));
	BSERIAL_CHECK_STATUS(bserial_blob_u64(ctx, buf, &wide));
	return bserial_store_len(ctx, len, type, wide);
}

bserial_status_t
bserial_bool(bserial_ctx_t* ctx, bool* boolean) {
	uint64_t u64 = *boolean;

	BSERIAL_CHECK_STATUS(bserial_uint(ctx, &u64));
	if (u64 <= 1) {
		*boolean = (bool)u64;
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
bserial_blob_u64(bserial_ctx_t* ctx, char* buf, uint64_t* len) {
	uint64_t actual_len = *len;
	BSERIAL_CHECK_STATUS(bserial_blob_header_u64(ctx, &actual_len));
	if (actual_len > *len) { return bserial_malformed(ctx); }
	*len = actual_len;

	BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, buf));

	return BSERIAL_OK;
}

bserial_status_t
bserial_blob_header_u64(bserial_ctx_t* ctx, uint64_t* len) {
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

// chibihash {{{

// small, fast 64 bit hash function (version 2).
//
// https://github.com/N-R-K/ChibiHash
//
// This is free and unencumbered software released into the public domain.
// For more information, please refer to <https://unlicense.org/>

static inline uint64_t
bserial__chibihash64__load32le(const uint8_t *p) {
	return (uint64_t)p[0] <<  0 | (uint64_t)p[1] <<  8 |
	       (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24;
}

static inline uint64_t
bserial__chibihash64__load64le(const uint8_t *p) {
	return bserial__chibihash64__load32le(p) | (bserial__chibihash64__load32le(p+4) << 32);
}

static inline uint64_t
bserial__chibihash64__rotl(uint64_t x, int n) {
	return (x << n) | (x >> (-n & 63));
}

static inline uint64_t
bserial__chibihash64(const void *keyIn, ptrdiff_t len, uint64_t seed) {
	const uint8_t *p = (const uint8_t *)keyIn;
	ptrdiff_t l = len;

	const uint64_t K = UINT64_C(0x2B7E151628AED2A7); // digits of e
	uint64_t seed2 = bserial__chibihash64__rotl(seed-K, 15) + bserial__chibihash64__rotl(seed-K, 47);
	uint64_t h[4] = { seed, seed+K, seed2, seed2+(K*K^K) };

	// depending on your system unrolling might (or might not) make things
	// a tad bit faster on large strings. on my system, it actually makes
	// things slower.
	// generally speaking, the cost of bigger code size is usually not
	// worth the trade-off since larger code-size will hinder inlinability
	// but depending on your needs, you may want to uncomment the pragma
	// below to unroll the loop.
	//#pragma GCC unroll 2
	for (; l >= 32; l -= 32) {
		for (int i = 0; i < 4; ++i, p += 8) {
			uint64_t stripe = bserial__chibihash64__load64le(p);
			h[i] = (stripe + h[i]) * K;
			h[(i+1)&3] += bserial__chibihash64__rotl(stripe, 27);
		}
	}

	for (; l >= 8; l -= 8, p += 8) {
		h[0] ^= bserial__chibihash64__load32le(p+0); h[0] *= K;
		h[1] ^= bserial__chibihash64__load32le(p+4); h[1] *= K;
	}

	if (l >= 4) {
		h[2] ^= bserial__chibihash64__load32le(p);
		h[3] ^= bserial__chibihash64__load32le(p + l - 4);
	} else if (l > 0) {
		h[2] ^= p[0];
		h[3] ^= p[l/2] | ((uint64_t)p[l-1] << 8);
	}

	h[0] += bserial__chibihash64__rotl(h[2] * K, 31) ^ (h[2] >> 31);
	h[1] += bserial__chibihash64__rotl(h[3] * K, 31) ^ (h[3] >> 31);
	h[0] *= K; h[0] ^= h[0] >> 31;
	h[1] += h[0];

	uint64_t x = (uint64_t)len * K;
	x ^= bserial__chibihash64__rotl(x, 29);
	x += seed;
	x ^= h[1];

	x ^= bserial__chibihash64__rotl(x, 15) ^ bserial__chibihash64__rotl(x, 42);
	x *= K;
	x ^= bserial__chibihash64__rotl(x, 13) ^ bserial__chibihash64__rotl(x, 31);

	return x;
}

// }}}

static inline uint64_t
bserial_hash(const void* data, size_t len) {
	return bserial__chibihash64(data, (ptrdiff_t)len, 0);
}

// https://nullprogram.com/blog/2022/08/08/
static inline int32_t
bserial_lookup_index(uint64_t hash, int32_t exp, int32_t idx) {
	uint32_t mask = ((uint32_t)1 << exp) - 1;
	uint32_t step = (uint32_t)((hash >> (64 - exp)) | 1);
	return (idx + step) & mask;
}

static inline bserial_schema_t*
bserial_alloc_schema(bserial_ctx_t* ctx, uint32_t num_fields) {
	if (ctx->num_schemas >= ctx->config.max_num_schemas) { return NULL; }
	if (num_fields > ctx->config.max_record_fields) { return NULL; }

	bserial_schema_t* schema = &ctx->schemas[ctx->num_schemas];
	schema->fields = ctx->schema_fields + (size_t)ctx->num_schemas * ctx->config.max_record_fields;
	schema->num_fields = num_fields;
	ctx->num_schemas += 1;
	return schema;
}

// Read a schema definition or reference. The marker has already been read.
static inline bserial_status_t
bserial_read_schema(bserial_ctx_t* ctx, uint8_t marker, bserial_schema_t** out) {
	if (marker == BSERIAL_RECORD_DEF) {
		uint64_t num_fields;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&num_fields, ctx->in));
		if (num_fields > ctx->config.max_record_fields) { return bserial_malformed(ctx); }

		bserial_schema_t* schema = bserial_alloc_schema(ctx, (uint32_t)num_fields);
		if (schema == NULL) { return bserial_malformed(ctx); }

		for (uint64_t i = 0; i < num_fields; ++i) {
			const char* symbol;
			uint64_t symbol_len;
			BSERIAL_CHECK_STATUS(bserial_symbol(ctx, &symbol, &symbol_len));
			schema->fields[i] = (bserial_symbol_t){ .buf = (char*)symbol, .len = symbol_len };
		}

		*out = schema;
		return BSERIAL_OK;
	} else if (marker == BSERIAL_RECORD_REF) {
		uint64_t id;
		BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&id, ctx->in));
		if (id >= ctx->num_schemas) { return bserial_malformed(ctx); }

		*out = &ctx->schemas[id];
		return BSERIAL_OK;
	} else {
		return bserial_malformed(ctx);
	}
}

static inline uint64_t
bserial_schema_hash(const bserial_record_mapping_t* keys, uint64_t num_keys) {
	uint64_t hash = num_keys;
	for (uint64_t i = 0; i < num_keys; ++i) {
		uint64_t key_hash = bserial_hash(keys[i].symbol, keys[i].symbol_len);
		hash ^= key_hash + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
	}
	return hash;
}

static inline bool
bserial_schema_eq(const bserial_schema_t* schema, const bserial_record_mapping_t* keys, uint64_t num_keys) {
	if (schema->num_fields != num_keys) { return false; }

	for (uint64_t i = 0; i < num_keys; ++i) {
		if (
			schema->fields[i].len != keys[i].symbol_len
			|| memcmp(schema->fields[i].buf, keys[i].symbol, keys[i].symbol_len) != 0
		) {
			return false;
		}
	}

	return true;
}

// Write a schema definition the first time a list of keys is seen and a
// reference afterwards.
static inline bserial_status_t
bserial_write_schema(
	bserial_ctx_t* ctx,
	const bserial_record_mapping_t* keys,
	uint64_t num_keys,
	bserial_schema_t** out
) {
	uint64_t hash = bserial_schema_hash(keys, num_keys);
	for (int32_t i = (int32_t)hash;;) {
		i = bserial_lookup_index(hash, ctx->schema_exp, i);
		int32_t index = ctx->schema_index[i];
		if (index == 0) {
			bserial_schema_t* schema = bserial_alloc_schema(ctx, (uint32_t)num_keys);
			if (schema == NULL) { return bserial_malformed(ctx); }
			ctx->schema_index[i] = (int32_t)ctx->num_schemas;  // id + 1

			uint8_t marker = BSERIAL_RECORD_DEF;
			BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
			BSERIAL_CHECK_STATUS(ctx->status = bserial_write_uint(num_keys, ctx->out));
			for (uint64_t j = 0; j < num_keys; ++j) {
				const char* symbol = keys[j].symbol;
				uint64_t symbol_len = keys[j].symbol_len;
				// This interns the symbol so the pointer is stable afterwards
				BSERIAL_CHECK_STATUS(bserial_symbol(ctx, &symbol, &symbol_len));
				schema->fields[j] = (bserial_symbol_t){ .buf = (char*)symbol, .len = symbol_len };
			}

			*out = schema;
			return BSERIAL_OK;
		} else if (bserial_schema_eq(&ctx->schemas[index - 1], keys, num_keys)) {
			uint8_t marker = BSERIAL_RECORD_REF;
			BSERIAL_CHECK_STATUS(ctx->status = bserial_write(ctx->out, &marker, sizeof(marker)));
			BSERIAL_CHECK_STATUS(ctx->status = bserial_write_uint((uint64_t)index - 1, ctx->out));

			*out = &ctx->schemas[index - 1];
			return BSERIAL_OK;
		}
	}
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
				BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&len, ctx->in));
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
				BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&len, ctx->in));
				if (len > 0 && depth == 0) { return bserial_malformed(ctx); }

				for (uint64_t i = 0; i < len; ++i) {
					BSERIAL_CHECK_STATUS(bserial_skip_next(ctx, depth - 1));
				}
			}
			break;
		case BSERIAL_TABLE:
			{
				bserial_discard_marker(ctx);

				uint64_t len;
				BSERIAL_CHECK_STATUS(ctx->status = bserial_read_uint(&len, ctx->in));
				if (len > 0) {
					// Every row is a record nested in the table
					if (depth < 2) { return bserial_malformed(ctx); }

					// The schema is stored once with the first row and must
					// be registered like any other definition.
					uint8_t schema_marker;
					BSERIAL_CHECK_STATUS(bserial_read_marker(ctx, &schema_marker));
					bserial_schema_t* schema;
					BSERIAL_CHECK_STATUS(bserial_read_schema(ctx, schema_marker, &schema));

					for (uint64_t i = 0; i < len; ++i) {
						for (uint32_t j = 0; j < schema->num_fields; ++j) {
							BSERIAL_CHECK_STATUS(bserial_skip_next(ctx, depth - 2));
						}
					}
				}
			}
			break;
		case BSERIAL_RECORD_DEF:
		case BSERIAL_RECORD_REF:
			{
				bserial_discard_marker(ctx);

				// Definitions must be registered even when skipped so that
				// later references resolve to the same ids as the writer's.
				bserial_schema_t* schema;
				BSERIAL_CHECK_STATUS(bserial_read_schema(ctx, marker, &schema));
				if (schema->num_fields > 0 && depth == 0) { return bserial_malformed(ctx); }

				for (uint32_t i = 0; i < schema->num_fields; ++i) {
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
bserial_array_u64(bserial_ctx_t* ctx, uint64_t* len) {
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
bserial_table_u64(bserial_ctx_t* ctx, uint64_t* len) {
	BSERIAL_CHECK_STATUS(bserial_begin_op(ctx, BSERIAL_OP_TABLE));
	BSERIAL_CHECK_STATUS(bserial_marker_and_length(ctx, BSERIAL_TABLE, len));

	if (*len > 0) {
		ctx->scope->len = *len;
		return BSERIAL_OK;
	} else {
		return bserial_end_op(ctx, BSERIAL_OP_TABLE);
	}
}

// The table that a record is a row of, if any.
// The first row carries the schema of the whole table.
static inline bserial_scope_t*
bserial_row_table(bserial_scope_t* record_scope) {
	// A record is never the root so it always has a parent
	bserial_scope_t* parent = record_scope - 1;
	return parent->type == BSERIAL_SCOPE_TABLE ? parent : NULL;
}

static inline bool
bserial_is_first_row(bserial_scope_t* table_scope) {
	// bserial_begin_op has already counted the current row
	return table_scope->iterator == 1;
}

bool
bserial_record(bserial_ctx_t* ctx) {
	if (ctx->status != BSERIAL_OK) { return false; }

	bserial_scope_t* scope = ctx->scope;
	// A nested record can only appear as the value of a key.
	// Otherwise, this is the loop head of the current record.
	bool loop_head = scope->type == BSERIAL_SCOPE_RECORD && !scope->value_pending;

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		if (loop_head) {
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
			if (bserial_begin_op(ctx, BSERIAL_OP_RECORD) != BSERIAL_OK) {
				return false;
			}
			scope = ctx->scope;

			bserial_schema_t* schema;
			bserial_scope_t* table = bserial_row_table(scope);
			if (table != NULL && !bserial_is_first_row(table)) {
				// Later rows of a table only contain values
				schema = table->table_schema;
			} else {
				uint8_t marker;
				if (bserial_read_marker(ctx, &marker) != BSERIAL_OK) {
					return false;
				}

				if (bserial_read_schema(ctx, marker, &schema) != BSERIAL_OK) {
					return false;
				}

				if (table != NULL) {
					table->table_schema = schema;
				}
			}

			// The mapping from keys to code is per record instance since the
			// same schema can be read by different functions.
			scope->len = schema->num_fields;
			for (uint32_t i = 0; i < schema->num_fields; ++i) {
				scope->record_schema[i] = (bserial_record_mapping_t){
					.symbol = schema->fields[i].buf,
					.symbol_len = schema->fields[i].len,
					.field_name = NULL,
				};
			}

			scope->record_mode = BSERIAL_RECORD_KEY_IO;
			scope->iterator = 0;
			return true;
		}
	} else {
		if (loop_head) {
			switch (scope->record_mode) {
				case BSERIAL_RECORD_MEASURE_WIDTH:
					if (scope->len > ctx->config.max_record_fields) {
						bserial_malformed(ctx);
						return false;
					}
					scope->record_mode = BSERIAL_RECORD_KEY_IO;
					scope->iterator = 0;
					return true;
				case BSERIAL_RECORD_KEY_IO:
					// The body must declare the same keys in every pass
					if (scope->iterator != scope->len) {
						bserial_malformed(ctx);
						return false;
					}
					{
						bserial_scope_t* table = bserial_row_table(scope);
						if (table != NULL && !bserial_is_first_row(table)) {
							// Later rows of a table must match the first
							if (!bserial_schema_eq(table->table_schema, scope->record_schema, scope->len)) {
								bserial_malformed(ctx);
								return false;
							}
						} else {
							bserial_schema_t* schema;
							if (bserial_write_schema(ctx, scope->record_schema, scope->len, &schema) != BSERIAL_OK) {
								return false;
							}

							if (table != NULL) {
								table->table_schema = schema;
							}
						}
					}
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
			if (bserial_begin_op(ctx, BSERIAL_OP_RECORD) != BSERIAL_OK) {
				return false;
			}
			scope = ctx->scope;
			scope->record_mode = BSERIAL_RECORD_MEASURE_WIDTH;
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
					scope->value_pending = true;
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
				// Collect the keys, the schema is written once all are known
				if (scope->iterator >= scope->len) {
					bserial_malformed(ctx);
					return false;
				}
				scope->record_schema[scope->iterator] = (bserial_record_mapping_t){
					.symbol = name,
					.symbol_len = len,
				};
				++scope->iterator;
				return false;
			case BSERIAL_RECORD_VALUE_IO:
				++scope->iterator;
				scope->value_pending = true;
				return true;
			default:
				bserial_malformed(ctx);
				return false;
		}
	}
}

// Widen an enum value of any storage type to int64_t
static inline bool
bserial_load_enum(const void* value, bserial_int_type_t type, int64_t* out) {
	if (type.is_signed) {
		return bserial_load_sint(value, type.size, out);
	} else {
		uint64_t wide;
		if (!bserial_load_uint(value, type.size, &wide)) { return false; }
		if (wide > INT64_MAX) { return false; }
		*out = (int64_t)wide;
		return true;
	}
}

// Store with range check
static inline bool
bserial_store_enum(void* value, bserial_int_type_t type, int64_t in) {
	if (type.is_signed) {
		return bserial_store_sint(value, type.size, in);
	} else {
		if (in < 0) { return false; }
		return bserial_store_uint(value, type.size, (uint64_t)in);
	}
}

bool
bserial_typed_enum(bserial_ctx_t* ctx, void* value, bserial_int_type_t type) {
	if (ctx->status != BSERIAL_OK) { return false; }

	bserial_scope_t* scope = ctx->scope;

	// Variants never contain values so an enum scope can only mean the end of
	// the loop.
	if (scope->type == BSERIAL_SCOPE_ENUM) {
		if (!scope->enum_matched) {
			// Writing a value with no name or reading a name with no variant
			bserial_malformed(ctx);
			return false;
		}

		bserial_end_op(ctx, BSERIAL_OP_ENUM);
		return false;
	}

	if (bserial_begin_op(ctx, BSERIAL_OP_ENUM) != BSERIAL_OK) {
		return false;
	}
	scope = ctx->scope;
	scope->enum_value = value;
	scope->enum_type = type;

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		if (bserial_symbol(ctx, &scope->enum_symbol, &scope->enum_symbol_len) != BSERIAL_OK) {
			return false;
		}
	}

	return true;
}

bserial_status_t
bserial_variant(bserial_ctx_t* ctx, const char* name, uint64_t len, int value) {
	BSERIAL_CHECK_STATUS(ctx->status);

	bserial_scope_t* scope = ctx->scope;
	if (scope->type != BSERIAL_SCOPE_ENUM) {
		return bserial_malformed(ctx);
	}

	// Only the first match counts
	if (scope->enum_matched) { return BSERIAL_OK; }

	if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
		if (
			scope->enum_symbol_len == len
			&& memcmp(scope->enum_symbol, name, len) == 0
		) {
			if (!bserial_store_enum(scope->enum_value, scope->enum_type, value)) {
				return bserial_malformed(ctx);
			}
			scope->enum_matched = true;
		}
	} else {
		int64_t current;
		if (!bserial_load_enum(scope->enum_value, scope->enum_type, &current)) {
			return bserial_malformed(ctx);
		}
		if (current == value) {
			BSERIAL_CHECK_STATUS(bserial_symbol(ctx, &name, &len));
			scope->enum_matched = true;
		}
	}

	return BSERIAL_OK;
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
			case BSERIAL_SCOPE_ENUM:
				bserial_tracef(tracer, userdata, depth, "Enum(%s)", scope->enum_matched ? "matched" : "unmatched");
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
	return fseek(((bserial_stdio_in_t*)in)->file, (long)size, SEEK_CUR) == 0;
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
