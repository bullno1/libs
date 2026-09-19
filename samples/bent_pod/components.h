#ifndef BENT_SAMPLE_COMPONENTS_H
#define BENT_SAMPLE_COMPONENTS_H

// The components of a program that is split into several source files.
// This header is included by every one of them:
//
// * components.c implements the registrations
// * movement.c owns the position component
// * main.c uses the components and saves them

// Customizations must be the same in every unit so they live here.
// This one lets the serialization callbacks receive a typed context.
typedef struct sample_io_s sample_io_t;
#define BENT_SERIALIZE_CTX sample_io_t
#include "../../bent.h"

// Note: Ignore the `//! [label]` markers, they are for for Doxygen
// They are pushed out of the way so you don't need to pay attention to them

//!                                                                             [BENT_POD_COMP_EX]
typedef struct { int frame; } animation_t;
typedef struct { bent_t entity; } target_t;

// Mutable helpers: bent_add_animation and bent_get_animation return `animation_t*`.
// Never saved.
BENT_POD_COMP_EX(animation, animation_t, RW, TRANSIENT)

// Mutable helpers, saved through bent_serialize_target which this declares
BENT_POD_COMP_EX(target, target_t, RW, SERIALIZED)
//!                                                                             [BENT_POD_COMP_EX]

//!                                                                             [BENT_POD_COMP_EX_RO]
typedef struct { int x, y; } position_t;

// Both bent_add_position and bent_get_position return `const position_t*`.
// Saved through bent_serialize_position.
BENT_POD_COMP_EX(position, position_t, RO, SERIALIZED)

// Other systems ask the owner to write on their behalf
void
movement_move(bent_world_t* world, bent_t entity, int dx, int dy);
//!                                                                             [BENT_POD_COMP_EX_RO]

#endif
