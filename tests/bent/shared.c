#include "shared.h"

BENT_DEFINE_COMP(basic_component) = {
	.size = sizeof(int),
	.flags = BENT_COMP_TRANSIENT,
};

BENT_DEFINE_COMP(basic_component2) = {
	.size = sizeof(float),
	.flags = BENT_COMP_TRANSIENT,
};

#define BLIB_IMPLEMENTATION
#include "../../bent.h"
