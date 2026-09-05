# bsfn

Stable function pointers for hot-reloadable modules.

`BSFN(fn)` returns a pointer to a small stub that survives module reloads;
`bsfn_reload` repoints every stub of the calling image at its current
functions.

`basic.c` and `no_reload.c` are part of the aggregate `tests` binary.
The `reload/` directory contains a standalone test that performs an actual
`dlopen`/`dlclose` reload: the same module source is built into two shared
libraries (v1 and v2) and the host verifies that the stable pointer obtained
from v1 reaches the v2 code after a reload.
Build and run it with:

```sh
make bin/bsfn && ./bin/bsfn
```
