#define BENT_DEFINE_COMPONENTS
#include "custom_ex.h"

BENT_INITIALIZER(cx_gauge) {
	_Static_assert(
		_Generic(world, bent_world_t*: 1, default: 0),
		"world must be the world the component is added to"
	);
	_Static_assert(
		_Generic(comp, cx_gauge_t*: 1, default: 0),
		"comp must be the component's own type"
	);
	_Static_assert(
		_Generic(arg, const cx_gauge_t*: 1, default: 0),
		"arg must be a const pointer to the argument type"
	);
	*comp = (cx_gauge_t){ 0 };
	if (arg != NULL) {
		comp->max = arg->max;
		comp->current = arg->max + arg->current;
	}
}

BENT_INITIALIZER(cx_counter) {
	_Static_assert(
		_Generic(arg, const cx_counter_arg_t*: 1, default: 0),
		"arg must be a const pointer to the argument type"
	);
	(void)world;
	comp->value = arg != NULL ? arg->start : -1;
	comp->num_inits = 1;
}
