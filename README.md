# blibs

[![Build status](https://github.com/bullno1/libs/actions/workflows/build.yml/badge.svg)](https://github.com/bullno1/libs/actions/workflows/build.yml)

Collection of miscellaneous single-header libraries.

|Library|Description|
|-------|-----------|
|[autolist.h](autolist.h)|A list of items collected from all compilation units|
|[xincbin.h](xincbin.h)|A cross-platform way to include binary data in your executable|
|[bresmon.h](bresmon.h)|File watcher|
|[mem_layout.h](mem_layout.h)|Combine multiple mallocs of a nested struct into one|
|[barena.h](barena.h)|Arena allocator|
|[tlsf.h](tlsf.h)|Adaptation of [jserv/tlsf-bsd](https://github.com/jserv/tlsf-bsd)|
|[bhash.h](bhash.h)|Type-safe hashtable|
|[bhamt.h](bhamt.h)|Type-safe hash trie, an arena-friendly associative map|
|[barray.h](barray.h)|Dynamic array|
|[bseg.h](bseg.h)|Segmented array|
|[bco.h](bco.h)|Coroutine|
|[bserial.h](bserial.h)|Binary serialization|
|[bsv.h](bsv.h)|Binary serialization with explicit versioning|
|[bspscq.h](bspscq.h)|Single producer single consumer queue|
|[barg.h](barg.h)|Command line argument parser|
|[bscn.h](bscn.h)|Text scanner for hand-written parsers|
|[bsfn.h](bsfn.h)|Stable function pointers for hot-reloadable modules|
|[bmacro.h](bmacro.h)|Commonly used macros|
|[bminmax.h](bminmax.h)|Min/Max/Clamp macros using `_Generic`|
|[blog.h](blog.h)|Logging, with short filenames|
|[qoi.h](qoi.h)|Quite OK image encoding/decoding|
|[bstacktrace.h](bstacktrace.h)|Portable stacktrace with source mapping|
|[bcrash_handler.h](bcrash_handler.h)|Crash handler|

The following libraries are not self-contained.
They depend on other libraries in this repository.

|Library|Description|
|-------|-----------|
|[btest.h](btest.h)|Unit testing framework with automatic test registration|
|[bent.h](bent.h)|Hot reload aware entity component system|

Examples for each library live in the corresponding `tests/<name>/` directory.

Tested on:

* Linux (GCC+Clang)
* Windows (MSVC)

For documentation, see: https://bullno1.com/libs

## On allocator
<!--! \anchor allocator -->

Whenever a library needs to allocate memory a `memctx` argument can be passed to it.
By default it uses libc for memory and `memctx` is ignored.

The macro `<NAME>_REALLOC` can be used to override the allocator.
`BLIB_REALLOC` is also recognized as a catch-all allocator.
