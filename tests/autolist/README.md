# autolist

For when you have a list of things (test cases, metadata...) spread around in different compilation units and you need to iterate them all.

See the test for more info.
`main.c` can simply use `AUTOLIST_DEFINE`, `AUTOLIST_BEGIN` and `AUTOLIST_END` to iterate over all entries declared as `AUTOLIST_ENTRY` in both `a.c` and `b.c`.
