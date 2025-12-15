#include <stdio.h>

typedef struct {
	float x;
	float y;
} vec2_t;

typedef struct {
	vec2_t position;
	vec2_t velocity;
} entity_t;

#define BSV_CUSTOM_TYPES(X) \
	X(vec2_t, bsv_vec2) \
	X(entity_t, bsv_entity)

#include "../../bsv.h"

bsv_status_t
bsv_vec2(bsv_ctx_t* ctx, vec2_t* vec2) {
	bsv_auto(ctx, &vec2->x);
	bsv_auto(ctx, &vec2->y);

	return bsv_status(ctx);
}

bsv_status_t
bsv_entity(bsv_ctx_t* ctx, entity_t* entity) {
	BSV_BLK(ctx, 0) {
		BSV_REV(0) {
			BSV_ADD(&entity->position);
			BSV_ADD(&entity->velocity);
		}
	}

	return bsv_status(ctx);
}

typedef struct {
	int depth;
} explain_ctx_t;

static void
print_explain(const bsv_explain_t* explain, void* userdata) {
	explain_ctx_t* ctx = userdata;
	switch (explain->type) {
		case BSV_EXPLAIN_ROOT: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*s{\n", ctx->depth++, "");
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_BLK: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*sblk[v:%d] {\n", ctx->depth++, "", explain->version);
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_REV: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*srev[v:%d] {\n", ctx->depth++, "", explain->version);
		   } else {
			   printf("%*s}\n", --ctx->depth, "");
		   }
		} break;
		case BSV_EXPLAIN_ARRAY: {
		} break;
		case BSV_EXPLAIN_ADD: {
		   if (explain->scope == BSV_EXPLAIN_BEGIN_SCOPE) {
			   printf("%*s+%s {\n", ctx->depth++, "", explain->name);
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
