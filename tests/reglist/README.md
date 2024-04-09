# reglist

For when you have a list of things (test cases, metadata...) spread around in different compilation units and you need to iterate them all.

See the test for more info.
`main.c` can simply use `REGLIST_DECLARE`, `REGLIST_BEGIN` and `REGLIST_END` to iterate over all entries declared as `REGLIST_ENTRY` in both `a.c` and `b.c`.
