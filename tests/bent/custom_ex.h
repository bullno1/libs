#ifndef BENT_TEST_CUSTOM_EX_H
#define BENT_TEST_CUSTOM_EX_H

#include "../../bent.h"

typedef struct { int current, max; } cx_gauge_t;
typedef struct { int value; int num_inits; } cx_counter_t;
typedef struct { int start; } cx_counter_arg_t;

BENT_CUSTOM_COMP_EX(cx_gauge, cx_gauge_t, cx_gauge_t, RO, SERIALIZED)

BENT_TRANSIENT_CUSTOM_COMP(cx_counter, cx_counter_t, cx_counter_arg_t)

#endif
