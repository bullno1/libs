# bserial

A binary serialization library.

## Motivation

I need a serialization library with the following criteria:

* Binary: No text parsing.
* No separate schema file: The serialization code is the schema.
  There is only a single function for both reading and writing.
* No separate object tree: There is only one phase: read/write data from the program's object directly to the input/output stream.
  This is different from many other libraries where a separate object tree (e.g: JSON) is created.
  Then, there is another phase to convert from that tree to internal representation.
* Schema evolution: The structure of objects can change.
  New keys can be added/removed/reordered.
  Existing keys can have their type changed.
  Old serialization files must still be compatible with newer versions.
* Fixed allocation: It should allocate a fixed amount of memory upfront.
  The memory complexity only depends on how deep the object tree can be, not how wide it is.
  There is always a bound check and invalid data is rejected.

## Design

There are several layers:

* Raw unstructed I/O: Functions to read/write varint and float in an endian-independent way.
  The functions generally have the forms:

  * `bserial_read_<type>(<type> value, bserial_in_t* in)`: Read a value from a stream.
  * `bserial_write_<type>(<type> value, bserial_out_t* out)`: Write a value to a stream.
* I/O stream implementations: The above functions work with an abstract input or output stream.
  Out of the box, there are concrete implementations for stdio `FILE` and memory stream.
* Structured serialization: The meat of the library.
  Functions generally have the signature: `bserial_<type>(bserial_ctx_t* ctx, <type>* value)`.
  `ctx` is the current serialization context.
  Depending on how it was constructed, it will either read or write `value` to its stream.
  Thus, the same code can be used for both serialization and deserialization, forming a "schema".

  `bserial_ctx_mem_size` is used to retrieve the fixed memory size needed for serialization/deserialization.
  Then `bserial_make_ctx` will be used to construct the context.
  Either an input stream or an output stream must be provided.

## Structured serialization

The supported data types are:

* signed/unsigned integers: They are stored as varint to save space since in practice, most integers are small.
  Internally, they are always converted into 64 bit.
  There are also convenient functions that does bound checking on read.
  e.g: A 8 bit unsgined integer will be checked to be in the range [0, 256].
  User code can do further range checking if needed.
* Floating points: Either single or double.
  IEEE 754 is assumed.
* Blob: An arbitrary binary blob of data.
  "String" is treated as blob in bserial.
  The library does not do any encoding validation.
* Symbol: Similar to strings but unique values are only written once.
  Subsequent occurences are referred to by id.
  For example, if the symbol `foo` has been written once as `[SYMBOL_DEF][foo]`, and then written again.
  `[SYMBOL_REF][0]` will be written to the stream instead.
  This helps to save space when certain sets of symbols are used repeatedly e.g: field names of a struct.
* Record: A collection of key-value pairs.
  The keys are always symbols.
  The values can have any types.
* Array: a collection of elements.
  Each element can have a different type.
* Table: a collection of elements.
  Unlike array, each element must be a record.
  All records must have the exact same set of keys.
  The storage is optimized for storing repeated records of the same type.

All data types are generally stored as `[tag][payload]`.
Variable length types like array or table are stored as `[tag][length][payload]`.
The type tag is needed to facilitate skipping over unknown data that is not described in the serialization code.

### Error handling

`BSERIAL_CHECK_STATUS` should be used on all functions that returns `bserial_status_t`.
This ensures that the code returns as soon as possible when error is encountered.

However, error is also "sticky".
As soon as a `bserial_ctx_t` encounters an error, all subsequent operations are noop.
This allows user code to trace the error with `bserial_trace`.

TODO: Improve `bserial_trace`.

### Record

Much of this library deals with reading and writing records in a backward compatible manner and allow for structural changes.
The serialization code for a struct must have the following form:

```c
bserial_status_t
serialize_my_struct(bserial_ctx_t* ctx, my_struct_t* my_struct) {
    while (bserial_record(ctx, my_struct)) {
        // Serialize field "foo"
        if (bserial_key(ctx, "foo", sizeof("foo") - 1)) {
            BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &my_struct->foo));
        }

        // Serialize field "bar"
        if (bserial_key(ctx, "bar", sizeof("bar") - 1)) {
            BSERIAL_CHECK_STATUS(bserial_f32(ctx, &my_struct->bar));
        }

        // Other fields...
    }

    return bserial_status(ctx);
}
```

Or with the helper macros:

```c
bserial_status_t
serialize_my_struct(bserial_ctx_t* ctx, my_struct_t* my_struct) {
    BSERIAL_RECORD(ctx, my_struct) {
        BSERIAL_KEY(ctx, foo) {
            BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &my_struct->foo));
        }

        // Serialize field "bar"
        BSERIAL_KEY(ctx, bar) {
            BSERIAL_CHECK_STATUS(bserial_f32(ctx, &my_struct->bar));
        }

        // Other fields...
    }

    return bserial_status(ctx);
}
```

The peculiar structure has to do with how the library handles record serialization.
Considering that it has to support the following:

* New fields may be added.
* Existing fields may be removed.
* Existing fields may be reordered.
* Existing fields may have their types changed.

The serialization code cannot be naively executed in order.

The outer `while` loop allows the library to run mulitple passes through the structure.
This allows it to, for example, store the keys separately from the values.

The inner `if` conditions allow the library to conditionally execute the value serialization code.
This allows it to reorder and toggle the execution of each field.
This can arguably be done with a function table where each key is a field name and each value is a serialization function.
However, it is much less ergonomic to do in C.
Even in C++, there are concerns about where the captures and the function table itself is allocated.
This violates the library's goal of fixed allocation.
In the common cases (code and data perfectly matches), the field clauses will most likely be executed in order anyway.
Thus there should not be much overhead.
Even with some omitted fields (due to newly introduced fields in code), given that all records in a table must have the same schema, the branches should be perfectly predictable.
e.g: "foo" is always executed and "bar" is always skipped.

#### Compatible type change

The above structure allows us to deal with compatible structural changes:

* Field reordering: The outer `while` loop allows field listing in any arbitrary order.
* Addition of new fields: The inner `if` conditions allow new fields in code but not in data to be toggled off.
* Removal of existing fields: Due to the way data is written, unrecogized fields will be automatically skipped over and not read.
  Internally, the call to `bserial_record` will skip to the closest recognized field in the data stream.
  As an optimization, after a field is read, the same skipping is done again.
  In the most common case where the data was written and then read by the same piece of code, there would be only two passes through the while loop:

  * The first pass is for schema discovery.
    Each `bserial_key` call in the code is mapped to a corresponding field in the data.
    They all return false to skip the value serialization code.
  * The second pass is for serialization.
    Since data was written in order, the first `bserial_key` call would return true.
    After the value is read, the library recognizes that the second `bserial_key` call is also a match in order and return true again, and so on...

  Thus there would be only two iterations through the `while`loop.
  In the case of a table, the schema discovery step only happens once at the beginning.
  For every row in the table, only a single iteration through the `while` loop is needed.

  The loop would only have to run more than twice when the program has to read serialized data from a previous version where fields have a different order.
  This can be somewhat mitigated by only adding code for new fields at the end of the serialization function.

#### Incompatible type change

Suppose that we have the following struct:

```c
typedef struct {
    int foo;
} my_struct_t;
```

And later we want to change the type into:

```c
typedef struct {
    float foo;
} my_struct_t;
```

This would be an incompatible change that the library cannot deal with out of the box.
However, the following idiom can be used:

```c
bserial_status_t
serialize_my_struct(bserial_ctx_t* ctx, my_struct_t* my_struct) {
    BSERIAL_RECORD(ctx, my_struct) {
        // Compatibility code path for reading from older versions
        if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
            // Read the old field as if it is the old version
            BSERIAL_KEY(ctx, foo) {
                int foo;  // We know that we are reading so
                          // `foo = my_struct->foo` is unnecessary.
                BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &foo));
                // Convert to the new version
                my_struct->foo = (float)foo;
            }
        }

        // For compatibility, when an incompatible change is made, a different
        // field name must be used in the serialized data.
        BSERIAL_KEY(ctx, foo_v2) {
            // The internal representation can use any field name, thus `foo` is
            // still legal.
            BSERIAL_CHECK_STATUS(bserial_float(ctx, &my_struct->foo));
        }
    }
}
```

Versioning can thus, be done per field instead of per record.
This should make backward-compatible schema change relatively painless.

### Variable length types

Beside the initial fixed-size memory buffer passed to `bserial_make_ctx`, the library does not allocate any more memory.
For variable-length data types such as table or array, user code has to make their own decision.
In general, fixed allocation is encouraged and it is always important to enforce a hard limit on length.
However, when dynamic size is needed something like this can be used:

```c
bserial_status_t
serialize_my_array(bserial_ctx_t* ctx, my_array_t* array) {  // The program's array type
    uint64_t len = (uint64_t)my_array_len(array);  // Put length into a variable
    BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));  // Serialize length
    if (len > MY_ARRAY_MAX_LEN) { return BSERIAL_MALFORMED; }  // Limit check
    my_array_resize(array, len);  // Dynamically resize the array to fit

    // Serialize body
    for (int i = 0; i < my_array_len(array); ++i) {
        BSERIAL_CHECK_STATUS(serialize_element(ctx, my_array_element_at(array, i)));
    }

    return bserial_status(ctx);
}
```

In the write path, `my_array_len_resize` should be a noop.
In the read path, `my_array_len_resize` will adjust the array to have the appropriate size.

In general, variable length type should follow the pattern of:

1. Assign length to a variable.
2. Read/write length with `bserial_array` or `bserial_table`.
3. Limit check
4. If using dynamic memory, resize storage to fit length.
5. Loop through elements and serialize them.

### Table

`bserial_array` and `bserial_table` have the same signature.
The only requirement for `bserial_table` is that all subsequent elements must be records of the same type: same set of keys and values must have the same type.
The storage optimization is done automatically.

Internally, the keys are only written once at the beginning of the table.
Then, all values of all rows are stored packed without separators.

### String encoding validation

This is considered to be out of scope.
However, it is not difficult to create a helper in user code:

```c
bserial_status_t
serialize_utf8_str(bserial_ctx_t* ctx, my_string_t* str) {  // The program's string type
    uint64_t len = (uint64_t)my_string_len(str);  // Put length into a variable
    BSERIAL_CHECK_STATUS(bserial_blob_header(ctx, &len));  // Serialize length
    if (len > MY_STRING_MAX_LEN) { return BSERIAL_MALFORMED; }  // Limit check
    my_string_resize(str, len);  // Dynamically resize the string

    BSERIAL_CHECK_STATUS(bserial_blob_body(ctx, my_string_cstr(str));  // Serialize string body
    // Assuming that all internal strings are valid, we only have to validate on read
    if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
        if (!validate_utf8(str)) { return BSERIAL_MALFORMED; }
    }

    return bserial_status(ctx);
}
```

### Variant/tagged union types

There is no built-in support for these types.
However, they could be emulated with an array where:

* The first element is the type tag.
* The second element is the payload.

```c
bserial_status_t
serialize_variant(bserial_ctx_t* ctx, my_variant_t* variant) {
    uint64_t len = 2;  // The size is always 2
    BSERIAL_CHECK_STATUS(bserial_array(ctx, &len));
    if (len != 2) { return BSERIAL_MALFORMED; }

    BSERIAL_CHECK_STATUS(bserial_any_int(ctx, &variant->type));
    switch (variant->type) {
        case MY_VARIANT_TYPE_1:
            return serialize_variant_type_1(ctx, &variant->payload.type1);
        case MY_VARIANT_TYPE_2:
            return serialize_variant_type_2(ctx, &variant->payload.type2);
        // ...
        default: // Always have a default case to guard against invalid data
            return BSERIAL_MALFORMED;
    }
}
```

Record is not appropriate for this usecase as fields in records are unordered.
There is no guarantee that the type field will be read before the payload fields.

### Associative map

Record is optimized for serializing a fixed set of keys and it is not suitable for associative map types such as hashtable.
Instead, a table should be used:

```c
bserial_status_t
serialize_hashmap(bserial_ctx_t* ctx, my_hashmap_t* hashmap) {
    uint64_t len = my_hashmap_len(hashmap);
    BSERIAL_CHECK_STATUS(bserial_table(ctx, &len));

    if (bserial_mode(ctx) == BSERIAL_MODE_READ) {
        // Optionally, reserve
        my_hashmap_reserve(hashmap, len);
        // Typically, on read, the input map should be empty but we can also clear it here
        my_hashmap_clear(hashmap)

        for (uint64_t i = 0; i < len; ++i) {
            my_hashmap_entry_t entry;
            BSERIAL_CHECK_STATUS(serialize_hashmap_entry(ctx, &entry));
            my_hashmap_put(hashmap, entry.key, entry.value);
        }
    } else {
        // Assuming this is the iteration API
        for (
            my_hashmap_entry_t* itr = my_hashmap_begin(hashmap);
            itr != my_hashmap_end(hashmap);
            itr = my_hasmap_next(itr)
        ) {
            BSERIAL_CHECK_STATUS(serialize_hashmap_entry(ctx, itr));
        }
    }

    return bserial_status(ctx);
}
```

Where `serialize_hashmap_entry` defines a record:

```c
bserial_status_t
serialize_hashmap_entry(bserial_ctx_t* ctx, my_hashmap_entry_t* entry) {
    BSERIAL_RECORD(ctx, entry) {
        BSERIAL_KEY(ctx, key) {
            BSERIAL_CHECK_STATUS(serialize_key_type(ctx, &entry->key));
        }

        BSERIAL_VALUE(ctx, value) {
            BSERIAL_CHECK_STATUS(serialize_value_type(ctx, &entry->value));
        }
    }

    return bserial_status(ctx);
}
```

The nature of hashmap is such that the write path and the read path have totally different code due to hashing.
We cannot quite use the same code for both reading and writing.
However, the entry serialization code can still be shared.
