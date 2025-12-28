#include "shared.h"

BENT_DEFINE_COMP(basic_component) = {
	.size = sizeof(int),
};

BENT_DEFINE_COMP(basic_component2) = {
	.size = sizeof(float),
};

#define BLIB_IMPLEMENTATION
#include "../../bent.h"
