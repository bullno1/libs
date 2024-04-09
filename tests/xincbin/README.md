# xincbin

Include arbitrary data in the executable.
Based on https://github.com/graphitemaster/incbin.
With some preprocessor abuse, this can also work in MSVC **without** any additional tool.

The trick is to define the resource in a `.rc` file with the `XINCBIN` macro.
A header file including the `.rc` file is also needed.
Finally, in a single source file, define `XINCBIN_IMPLEMENTATION` and include all the resource headers.
For retrieval, use the `XINCBIN_GET` macro.

Refer to the test for more info.
