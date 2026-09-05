// Built twice, with -DBSFN_MODULE_VARIANT=1 and =2, to simulate two versions
// of the same hot-reloadable module.
// The implementation is deliberately compiled into the module as well as the
// host: the duplicated code operates on the shared registry.
#define BSFN_IMPLEMENTATION
#include "../../../bsfn.h"

#ifndef BSFN_MODULE_VARIANT
#define BSFN_MODULE_VARIANT 0
#endif

static int
version(void) {
	return BSFN_MODULE_VARIANT;
}

int (*module_entry(bsfn_ctx_t* ctx))(void) {
	bsfn_bind(ctx);
	return BSFN(version);
}
