# blibs

Collection of miscellaneous single-header libraries.

|Library|Description|
|-------|-----------|
|[autolist.h](tests/autolist)|A list of items collected from all compilation units|
|[xincbin.h](tests/xincbin)|A cross-platform way to include binary data in your executable|
|[bresmon.h](tests/bresmon)|File watcher|
|[mem_layout.h](tests/mem_layout)|Combine multiple mallocs of a nested struct into one|
|[barena.h](tests/barena)|Arena allocator|
|[tlsf.h](tests/tlsf)|Adaptation of [jserv/tlsf-bsd](https://github.com/jserv/tlsf-bsd)|
|[bhash.h](tests/bhash)|Hashtable|
|[bcoro.h](tests/bcoro)|Coroutine|
|[bserial.h](tests/bserial)|Binary serialization|
|[bspsc.h](tests/bspsc)|Single producer single consumer queue|

Each one has example and documentation in the corresponding tests directory.

Tested on:

* Linux (GCC+Clang)
* Windows (MSVC)

## On allocator

Whenever a library needs to allocate memory a `memctx` argument can be passed to it.
By default it uses libc for memory and `memctx` is ignored.

The macro `<NAME>_REALLOC` can be used to override the allocator.
`BLIB_REALLOC` is also recognized as a catch-all allocator.
