# bsfn

Stable function pointers for hot-reloadable modules.

`BSFN(fn)` returns a pointer to a small stub that survives module reloads;
`bsfn_reload` repoints every stub of the calling image at its current
functions.

The library is Linux-only so its tests are a standalone binary rather than
part of the aggregate `tests` binary.
Besides the unit tests, `reload.c` performs an actual `dlopen`/`dlclose`
reload: `module/module.c` is built into two shared libraries (v1 and v2) next
to the test executable, and the test verifies that the stable pointer
obtained from v1 reaches the v2 code after a reload.
Build and run it with:

```sh
make bin/bsfn && ./bin/bsfn
```
