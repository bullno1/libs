#include <stdio.h>

#define MAX_ITEMS 8

// First, we define some basic types

typedef struct {
	float x;
	float y;
} vec2_t;

typedef struct {
	int id;
	int count;
} item_t;

typedef struct {
	int num_items;
	item_t items[MAX_ITEMS];
} inventory_t;

typedef struct {
	vec2_t position;
	vec2_t velocity;
	int health;
	inventory_t inventory;
} entity_t;

// Belper for bsv_auto
//!                                                                             [BSV_CUSTOM_TYPES]
#define BSV_CUSTOM_TYPES(X) \
	X(vec2_t, bsv_vec2) \
	X(entity_t, bsv_entity)
//!                                                                             [BSV_CUSTOM_TYPES]

#include "../bsv.h"

// To serialize fundamental types, just use bsv_auto
//                                                                              [bsv_auto]
bsv_status_t
bsv_vec2(bsv_ctx_t* ctx, vec2_t* vec2) {
	bsv_auto(ctx, &vec2->x);
	bsv_auto(ctx, &vec2->y);

	return bsv_status(ctx);
}
//                                                                              [bsv_auto]

// To serialize a complex type, declare a function with this signature
//!                                                                             [BSV_BLK]
bsv_status_t
bsv_item(bsv_ctx_t* ctx, item_t* item) {
	BSV_BLK(ctx, 0) {  // The root is a BSV_BLK
		BSV_REV(0) {  // Each revision needs a BSV_REV
			BSV_ADD(&item->id);  // Add a field
			BSV_ADD(&item->count);
		}
	}

	return bsv_status(ctx);
}
//!                                                                             [BSV_BLK]

// To serialize an array, use BSV_ARRAY
//!                                                                             [BSV_ARRAY]
bsv_status_t
bsv_inventory(bsv_ctx_t* ctx, inventory_t* inv) {
	bsv_len_t len = inv->num_items;
	BSV_ARRAY(ctx, &len) {
		// TODO: bound check
		inv->num_items = (int)len;

		for (bsv_len_t i = 0; i < len; ++i) {
			BSV_CHECK_STATUS(bsv_item(ctx, &inv->items[i]));
		}
	}

	return bsv_status(ctx);
}
//!                                                                             [BSV_ARRAY]

// To update a record, add new revision blocks
//!                                                                             [BSV_REV]
bsv_status_t
bsv_entity(bsv_ctx_t* ctx, entity_t* entity) {
	BSV_BLK(ctx, 2) {
		BSV_REV(0) {  // Initial version
			BSV_ADD(&entity->position);
			BSV_ADD(&entity->velocity);
		}

		BSV_REV(1) {  // Later version
			BSV_ADD(&entity->health);
		}

		BSV_REV(2) {
			BSV_ADD_EX(&entity->inventory, bsv_inventory);
		}
	}

	return bsv_status(ctx);
}
//!                                                                             [BSV_REV]

#define bsv_entity bsv_entity2
// If we later remove entity_t.health in revision 3, edit the function into:
//!                                                                             [BSV_REM]
bsv_status_t
bsv_entity(bsv_ctx_t* ctx, entity_t* entity) {
	BSV_BLK(ctx, 2) {
		BSV_REV(0) {  // Initial version
			BSV_ADD(&entity->position);
			BSV_ADD(&entity->velocity);
		}

		BSV_REV(1) {  // Later version
			// Previously:
			// BSV_ADD(&entity->health);
			// Now we remove it:
			int health = 0;  // Because the field no longer exists, we use local
							 // variable.
			BSV_REM(&health, 2) {  // Backward compatibility conversion
				entity->health = health;
			}
		}

		BSV_REV(2) {
			BSV_ADD_EX(&entity->inventory, bsv_inventory);
		}
	}

	return bsv_status(ctx);
}
//!                                                                             [BSV_REM]
#undef bsv_entity

typedef struct {
	int depth;
} explain_ctx_t;

static void
print_explain(const bsv_explain_t* explain, void* userdata) {
	explain_ctx_t* ctx = userdata;
	switch (explain->type) {
		case BSV_EXPLAIN_ROOT: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*s{ // %s:%d\n", ctx->depth++, "", explain->file, explain->line);
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_BLK: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*sblk[v:%d] { // %s:%d\n", ctx->depth++, "", explain->version, explain->file, explain->line);
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_REV: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*srev[v:%d] { // %s:%d\n", ctx->depth++, "", explain->version, explain->file, explain->line);
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_ARRAY: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*s[ // %s:%d\n", ctx->depth++, "", explain->file, explain->line);
		   } else {
			   printf("%*s]\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_ADD: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*s%s { // %s:%d\n", ctx->depth++, "", explain->name, explain->file, explain->line);
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_REM: {
		} break;
		case BSV_EXPLAIN_RAW: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*s[%s]", ctx->depth, "", explain->function);
		   } else {
			   printf("\n");
		   }
		} break;
	}
}

int
main(int argc, const char* argv[]) {
	explain_ctx_t explain_ctx = { 0 };
	BSV_EXPLAIN(entity_t, bsv_auto, print_explain, &explain_ctx);
}

#define BLIB_IMPLEMENTATION
#include "../../bsv.h"
