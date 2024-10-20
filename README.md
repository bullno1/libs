Collection of miscellaneous single-header libraries.

|Library|Description|
|-------|-----------|
|[autolist.h](tests/autolist)|A list of items collected from all compilation units|
|[xincbin.h](tests/xincbin)|A cross-platform way to include binary data in your executable|
|[bresmon.h](tests/bresmon)|File watcher|
|[mem_layout.h](tests/mem_layout)|Combine multiple mallocs of a nested struct into one|
|[barena.h](tests/barena)|Arena allocator|
|[tlsf.h](tests/tlsf)|Adaptation of [jserv/tlsf-bsd](https://github.com/jserv/tlsf-bsd)|

Each one has example and documentation in the corresponding tests directory.

Tested on:

* Linux (GCC+Clang)
* Windows (MSVC)
