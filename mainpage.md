# blibs

Collection of miscellaneous single-header libraries.

|Library|Description|
|-------|-----------|
|@ref bresmon.h|File watcher|
|@ref bhash.h|Type-safe hashtable|
|@ref bcoro.h|Coroutine|
|@ref bserial.h|Serialization|
|@ref bspscq.h|Single producer single consumer queue|
|@ref bsv.h|Binary serialization with versioning|
|@ref bstacktrace.h|Portable stacktrace with source mapping|

The following libraries are not self-contained.
They depend on other libraries in this repository.

|Library|Description|
|-------|-----------|
|@ref btest.h|Unit testing framework with automatic test registration|
|@ref bent.h|Hot reload aware entity component system|

## On allocator
\anchor allocator

Whenever a library needs to allocate memory a `memctx` argument can be passed to it.
By default it uses libc for memory and `memctx` is ignored.

The macro `<NAME>_REALLOC` can be used to override the allocator.
`BLIB_REALLOC` is also recognized as a catch-all allocator.
