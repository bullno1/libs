# bsfn

Stable function pointers for hot-reloadable modules.

`BSFN(fn)` returns a pointer to a small stub that survives module reloads;
`bsfn_bind` repoints every stub of the calling image at its current
functions.
