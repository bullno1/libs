# blibs

[![Build status](https://github.com/bullno1/libs/actions/workflows/build.yml/badge.svg)](https://github.com/bullno1/libs/actions/workflows/build.yml)

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
|[barray.h](barray.h)|Dynamic array|
|[bcoro.h](tests/bcoro)|Coroutine|
|[bserial.h](tests/bserial)|Binary serialization|
|[bspsc.h](tests/bspscq)|Single producer single consumer queue|
|[barg.h](barg.h)|CLI argument parsing|
|[bmacro.h](bmacro.h)|Commonly used macros|
|[bminmax.h](bminmax.h)|Min/Max/Clamp macros using `_Generic`|
|[btest.h](btest.h)|Unit test framework (based on autolist)|
|[blog.h](blog.h)|Logging, with short filenames|
|[qoi.h](qoi.h)|Quite OK image encoding/decoding|
|[bent.h](bent.h)|Entity component system|
|[bsv.h](bsv.h)|Binary seriallization with explicit versioning|
|[bstacktrace.h](bstacktrace.h)|Portable stacktrace with source mapping|

Each one has example and documentation in the corresponding tests directory.

Tested on:

* Linux (GCC+Clang)
* Windows (MSVC)

## On allocator

Whenever a library needs to allocate memory a `memctx` argument can be passed to it.
By default it uses libc for memory and `memctx` is ignored.

The macro `<NAME>_REALLOC` can be used to override the allocator.
`BLIB_REALLOC` is also recognized as a catch-all allocator.
