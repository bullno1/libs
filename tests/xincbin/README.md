# xincbin

Include arbitrary data in the executable.
Based on https://github.com/graphitemaster/incbin.
With some preprocessor abuse, this can also work in MSVC **without** any additional tool.

The trick is to define the resource in a `.rc` file with the `XINCBIN` macro.
A header file including the `.rc` file is also needed.
Finally, in a single source file, define `XINCBIN_IMPLEMENTATION` and include all the resource headers.
For retrieval, use the `XINCBIN_GET` macro.

Every resource is implicitly null-terminated: `data[size]` is always 0 but the
terminator is not counted in `size`.
Text resources can thus be used as C strings without copying.
On the GNU-style assembly path this costs a single extra byte in `.rodata`.
On the Windows resource path (MSVC), where resources are stored verbatim,
`XINCBIN_GET` lazily makes a null-terminated copy once and caches it for the
lifetime of the process.

Refer to the test for more info.
