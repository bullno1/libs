// vim: set foldmethod=marker foldlevel=0:
#ifndef BASTAR_H
#define BASTAR_H

/**
 * @file
 * @brief A* path finding over a graph provided by the application.
 *
 * The library does not store a graph.
 * It discovers the world through a callback interface.
 *
 * ## Nodes
 *
 * A node is a @ref bastar_node_t: a 64-bit value chosen by the application.
 * The library only hashes it and compares it for equality.
 * It can be anything: a packed grid coordinate, a pointer, a handle or an index.
 * There is no special reserved value.
 *
 * ## Callbacks
 *
 * A search is described by a @ref bastar_query_t:
 *
 * * `expand` reports every edge leaving a node with @ref bastar_edge.
 * * `heuristic` estimates the remaining cost from a node to the goal.
 * * `is_goal` tells whether a node ends the search.
 *
 * The path is the cheapest one when `heuristic` never overestimates.
 *
 * ## Memory
 *
 * Memory comes from `BASTAR_ALLOC(size, align, memctx)`, an arena allocator.
 *
 * A @ref bastar_t owns nothing and needs no cleanup.
 *
 * ## Example
 *
 * @code{.c}
 * typedef struct { const grid_t* grid; int goal_x, goal_y; } grid_query_t;
 *
 * static inline bastar_node_t
 * grid_node(int x, int y) { return (uint64_t)(uint32_t)x << 32 | (uint32_t)y; }
 *
 * static void
 * grid_expand(void* userdata, bastar_t* astar, bastar_node_t node) {
 *     const grid_query_t* query = userdata;
 *     int x = (int)(node >> 32), y = (int)(uint32_t)node;
 *     static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
 *     for (int i = 0; i < 4; ++i) {
 *         int nx = x + dirs[i][0], ny = y + dirs[i][1];
 *         if (grid_is_walkable(query->grid, nx, ny)) {
 *             bastar_edge(astar, grid_node(nx, ny), 1.f);
 *         }
 *     }
 * }
 *
 * static bastar_cost_t
 * grid_manhattan(void* userdata, bastar_node_t node) {
 *     const grid_query_t* query = userdata;
 *     int x = (int)(node >> 32), y = (int)(uint32_t)node;
 *     return (bastar_cost_t)(abs(x - query->goal_x) + abs(y - query->goal_y));
 * }
 *
 * static bool
 * grid_at_goal(void* userdata, bastar_node_t node) {
 *     const grid_query_t* query = userdata;
 *     return node == grid_node(query->goal_x, query->goal_y);
 * }
 *
 * barena_snapshot_t snapshot = barena_snapshot(&arena);
 * bastar_t astar;
 * bastar_status_t status = bastar_find(&astar, &(bastar_query_t){
 *     .expand = grid_expand,
 *     .heuristic = grid_manhattan,
 *     .is_goal = grid_at_goal,
 *     .userdata = &(grid_query_t){ .grid = &grid, .goal_x = 31, .goal_y = 17 },
 *     .start = grid_node(0, 0),
 * }, &arena);
 * bastar_path_t path = bastar_path(&astar);  // Allocated from the arena
 * for (size_t i = 0; i < path.len; ++i) { walk_to(path.nodes[i]); }
 * barena_restore(&arena, snapshot);  // The search and the path are gone
 * @endcode
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef BASTAR_API
#define BASTAR_API
#endif

/**
 * @brief The type of an edge cost
 */
#ifndef BASTAR_COST_TYPE
#define BASTAR_COST_TYPE float
#endif

/// Actual meaning is defined by the application
typedef uint64_t bastar_node_t;

typedef BASTAR_COST_TYPE bastar_cost_t;

typedef struct bastar_s bastar_t;

// Callbacks {{{

/**
 * Report every edge leaving @p node by calling @ref bastar_edge on @p astar.
 *
 * A node with no edge to report is a dead end.
 */
typedef void (*bastar_expand_fn_t)(void* userdata, bastar_t* astar, bastar_node_t node);

/**
 * Estimate the cost of going from @p node to the goal.
 *
 * Must not be negative.
 * The path is the cheapest one when this never overestimates.
 */
typedef bastar_cost_t (*bastar_heuristic_fn_t)(void* userdata, bastar_node_t node);

/**
 * Whether reaching @p node ends the search.
 */
typedef bool (*bastar_goal_fn_t)(void* userdata, bastar_node_t node);

// }}}

// Query {{{

/**
 * A search to run.
 */
typedef struct {
	bastar_expand_fn_t expand;
	/// Optional. Without it, every estimate is 0: Dijkstra's algorithm.
	bastar_heuristic_fn_t heuristic;
	/**
	 * Optional. Without it, there is no goal: the search explores everything.
	 */
	bastar_goal_fn_t is_goal;
	/// Passed to every callback
	void* userdata;

	/// The starting node
	bastar_node_t start;
	/**
	 * The most nodes the search may discover, the start included.
	 * 0 means no limit.
	 *
	 * This bounds memory: a search never allocates more than this many
	 * records.
	 */
	size_t max_nodes;
} bastar_query_t;

// }}}

// Handle {{{

typedef enum {
	/// The budget of @ref bastar_step was spent
	BASTAR_RUNNING,
	/// A goal was reached
	BASTAR_FOUND,
	/// Everything reachable from the start was explored and none of it is a goal
	BASTAR_UNREACHABLE,
	/// `max_nodes` was reached or the allocator returned NULL
	BASTAR_GAVE_UP,
} bastar_status_t;

/**
 * A search.
 *
 * Exposed for static allocation.
 */
struct bastar_s {
	/// Current status
	bastar_status_t status;
	/// Nodes discovered so far, the start included
	size_t num_nodes;
	/// Calls to `expand` so far
	size_t num_expansions;

	// Internal
	bastar_query_t query;
	void* memctx;
	struct bastar_record_s* records;
	struct bastar_record_s* open;
	struct bastar_record_s* current;
	struct bastar_record_s* end;
	const bastar_node_t* path;
	size_t path_len;
};

// }}}

// Search {{{

/**
 * Begin a search.
 *
 * @param memctx Context passed to the allocator for the whole search.
 *
 * @return @ref BASTAR_RUNNING, or @ref BASTAR_GAVE_UP when the start node
 *   could not be allocated.
 */
BASTAR_API bastar_status_t
bastar_begin(bastar_t* astar, const bastar_query_t* query, void* memctx);

/**
 * Continue a search for at most @p max_expansions calls to `expand`.
 *
 * Once the status is anything other than @ref BASTAR_RUNNING, this does nothing and
 * returns it again.
 */
BASTAR_API bastar_status_t
bastar_step(bastar_t* astar, size_t max_expansions);

/// @ref bastar_begin, then @ref bastar_step until it stops running
BASTAR_API bastar_status_t
bastar_find(bastar_t* astar, const bastar_query_t* query, void* memctx);

/**
 * Report an edge from the node being expanded to @p neighbor.
 *
 * @p cost must not be negative. It may be 0.
 *
 * The same neighbor may be reported more than once, the cheapest edge wins.
 * An edge back to the node being expanded is ignored.
 *
 * Valid inside @ref bastar_expand_fn_t only, on the @ref bastar_t it was given.
 */
BASTAR_API void
bastar_edge(bastar_t* astar, bastar_node_t neighbor, bastar_cost_t cost);

// }}}

// Result {{{

/**
 * The node the path ends at.
 *
 * With @ref BASTAR_FOUND, this is the goal that was reached.
 * Otherwise, it is the discovered node with the lowest estimate, the nearest
 * to the start among equals.
 *
 * Valid any time after @ref bastar_begin, until the memory of the search is
 * released. While the search is running, it is the best node so far.
 */
BASTAR_API bastar_node_t
bastar_end(const bastar_t* astar);

/// The cost of the path from the start to @ref bastar_end
BASTAR_API bastar_cost_t
bastar_cost(const bastar_t* astar);

/// A path, in order from the start
typedef struct {
	const bastar_node_t* nodes;
	/// At least 1: a start that is also the goal is a path of 1
	size_t len;
} bastar_path_t;

/**
 * The path from the start to @ref bastar_end, both included.
 *
 * The first call allocates it from the `memctx` of the search.
 * Later calls return the same array, until @ref bastar_step advances the
 * search.
 *
 * When the allocator returns NULL, `nodes` is NULL and `len` is 0.
 */
BASTAR_API bastar_path_t
bastar_path(bastar_t* astar);

// }}}

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BASTAR_IMPLEMENTATION)
#define BASTAR_IMPLEMENTATION
#endif

#ifdef BASTAR_IMPLEMENTATION

#ifndef BASTAR_ALLOC
#	ifdef BLIB_ARENA_ALLOC
#		define BASTAR_ALLOC(size, align, ctx) BLIB_ARENA_ALLOC(size, align, ctx)
#	else
struct barena_s;
void*
barena_memalign(struct barena_s* arena, size_t size, size_t alignment);
#		define BASTAR_ALLOC(size, align, ctx) \
			barena_memalign((struct barena_s*)(ctx), (size), (align))
#	endif
#endif

#ifndef BASTAR_ASSERT
#include <assert.h>
#define BASTAR_ASSERT(COND, MSG) assert((COND) && (MSG))
#endif

#define BASTAR__TRIE_BITS 2
#define BASTAR__TRIE_CHILDREN (1 << BASTAR__TRIE_BITS)

/**
 * Everything known about a discovered node.
 *
 * It is at once a node of the hash trie that finds it by name and, while
 * open, a node of the pairing heap that orders the open set.
 * This is the only thing a search allocates and it never moves.
 */
typedef struct bastar_record_s bastar_record_t;

struct bastar_record_s {
	bastar_record_t* trie_children[BASTAR__TRIE_CHILDREN];
	bastar_node_t node;

	/// The cheapest known way here, NULL for the start
	bastar_record_t* parent;
	/// Cost from the start
	bastar_cost_t g;
	/// Estimate to the goal
	bastar_cost_t h;
	/// In the heap, waiting to be expanded
	bool open;

	bastar_record_t* heap_child;
	bastar_record_t* heap_sibling;
	/// The previous sibling, or the heap parent for a first child
	bastar_record_t* heap_prev;
};

// Hash trie {{{

// The finalizer of splitmix64.
// It is a bijection: two nodes never share a hash, which bounds the depth of
// the trie.
static inline uint64_t
bastar__hash(bastar_node_t node) {
	uint64_t hash = node;
	hash ^= hash >> 30;
	hash *= UINT64_C(0xbf58476d1ce4e5b9);
	hash ^= hash >> 27;
	hash *= UINT64_C(0x94d049bb133111eb);
	hash ^= hash >> 31;
	return hash;
}

// The slot holding the record of a node, or the empty slot where it belongs
static bastar_record_t**
bastar__find_slot(bastar_t* astar, bastar_node_t node) {
	bastar_record_t** slot = &astar->records;
	for (
		uint64_t hash = bastar__hash(node);
		*slot != NULL && (*slot)->node != node;
		hash <<= BASTAR__TRIE_BITS
	) {
		slot = &(*slot)->trie_children[hash >> (64 - BASTAR__TRIE_BITS)];
	}
	return slot;
}

// }}}

// Pairing heap {{{

// Whether a must be expanded before b
static inline bool
bastar__before(const bastar_record_t* a, const bastar_record_t* b) {
	bastar_cost_t fa = (bastar_cost_t)(a->g + a->h);
	bastar_cost_t fb = (bastar_cost_t)(b->g + b->h);
	if (fa < fb) { return true; }
	if (fb < fa) { return false; }
	// Among equals, the one farther from the start is closer to the goal
	return b->g < a->g;
}

// Both must be roots: no sibling and no prev
static bastar_record_t*
bastar__heap_meld(bastar_record_t* a, bastar_record_t* b) {
	if (a == NULL) { return b; }
	if (b == NULL) { return a; }

	if (bastar__before(b, a)) {
		bastar_record_t* tmp = a;
		a = b;
		b = tmp;
	}

	b->heap_prev = a;
	b->heap_sibling = a->heap_child;
	if (a->heap_child != NULL) { a->heap_child->heap_prev = b; }
	a->heap_child = b;
	return a;
}

static void
bastar__heap_push(bastar_t* astar, bastar_record_t* record) {
	record->open = true;
	record->heap_child = NULL;
	record->heap_sibling = NULL;
	record->heap_prev = NULL;
	astar->open = bastar__heap_meld(astar->open, record);
}

static void
bastar__heap_pop(bastar_t* astar) {
	bastar_record_t* top = astar->open;

	// A node can have as many children as there are open nodes, so both
	// passes are loops instead of the textbook recursion.

	// Meld the children two by two, from the left.
	// The results are chained in reverse through heap_sibling.
	bastar_record_t* pairs = NULL;
	bastar_record_t* child = top->heap_child;
	while (child != NULL) {
		bastar_record_t* first = child;
		bastar_record_t* second = first->heap_sibling;
		child = second != NULL ? second->heap_sibling : NULL;

		first->heap_sibling = NULL;
		first->heap_prev = NULL;
		if (second != NULL) {
			second->heap_sibling = NULL;
			second->heap_prev = NULL;
		}

		bastar_record_t* pair = bastar__heap_meld(first, second);
		pair->heap_sibling = pairs;
		pairs = pair;
	}

	// Meld the results one by one, from the right
	bastar_record_t* root = NULL;
	while (pairs != NULL) {
		bastar_record_t* next = pairs->heap_sibling;
		pairs->heap_sibling = NULL;
		root = bastar__heap_meld(root, pairs);
		pairs = next;
	}

	astar->open = root;
	top->open = false;
	top->heap_child = NULL;
}

// The record became cheaper: move it towards the top
static void
bastar__heap_decrease(bastar_t* astar, bastar_record_t* record) {
	if (record == astar->open) { return; }

	// Cut it out along with everything below it
	bastar_record_t* prev = record->heap_prev;
	if (prev->heap_child == record) {
		prev->heap_child = record->heap_sibling;
	} else {
		prev->heap_sibling = record->heap_sibling;
	}
	if (record->heap_sibling != NULL) { record->heap_sibling->heap_prev = prev; }
	record->heap_sibling = NULL;
	record->heap_prev = NULL;

	astar->open = bastar__heap_meld(astar->open, record);
}

// }}}

// Search {{{

// The best place to stop at, short of a goal
static inline void
bastar__consider_end(bastar_t* astar, bastar_record_t* record) {
	const bastar_record_t* end = astar->end;
	if (
		end == NULL
		|| record->h < end->h
		|| (!(end->h < record->h) && record->g < end->g)
	) {
		astar->end = record;
	}
}

// Give up when there is no room for one more node
static bastar_record_t*
bastar__discover(
	bastar_t* astar,
	bastar_record_t** slot,
	bastar_node_t node,
	bastar_record_t* parent,
	bastar_cost_t g
) {
	const bastar_query_t* query = &astar->query;

	bastar_record_t* record = NULL;
	if (query->max_nodes == 0 || astar->num_nodes < query->max_nodes) {
		record = BASTAR_ALLOC(sizeof(bastar_record_t), _Alignof(bastar_record_t), astar->memctx);
	}
	if (record == NULL) {
		astar->status = BASTAR_GAVE_UP;
		return NULL;
	}

	bastar_cost_t zero = 0;
	bastar_cost_t h = query->heuristic != NULL
		? query->heuristic(query->userdata, node)
		: zero;
	BASTAR_ASSERT(!(h < zero), "The heuristic must not be negative");

	*record = (bastar_record_t){
		.node = node,
		.parent = parent,
		.g = g,
		.h = h,
	};
	*slot = record;
	++astar->num_nodes;

	bastar__heap_push(astar, record);
	bastar__consider_end(astar, record);
	return record;
}

bastar_status_t
bastar_begin(bastar_t* astar, const bastar_query_t* query, void* memctx) {
	BASTAR_ASSERT(query->expand != NULL, "expand is required");

	*astar = (bastar_t){
		.status = BASTAR_RUNNING,
		.query = *query,
		.memctx = memctx,
	};
	bastar__discover(astar, &astar->records, query->start, NULL, 0);
	return astar->status;
}

bastar_status_t
bastar_step(bastar_t* astar, size_t max_expansions) {
	if (astar->status != BASTAR_RUNNING) { return astar->status; }
	BASTAR_ASSERT(astar->current == NULL, "bastar_step called from expand");

	// The path to the end can change even when the end does not
	astar->path = NULL;
	astar->path_len = 0;

	const bastar_query_t* query = &astar->query;
	for (;;) {
		bastar_record_t* top = astar->open;
		if (top == NULL) {
			astar->status = BASTAR_UNREACHABLE;
			break;
		}

		// Tested here and not on discovery: only now is the way to this node
		// known to be the cheapest
		if (query->is_goal != NULL && query->is_goal(query->userdata, top->node)) {
			astar->end = top;
			astar->status = BASTAR_FOUND;
			break;
		}

		if (max_expansions == 0) { break; }
		--max_expansions;

		bastar__heap_pop(astar);
		++astar->num_expansions;
		astar->current = top;
		query->expand(query->userdata, astar, top->node);
		astar->current = NULL;

		if (astar->status != BASTAR_RUNNING) { break; }
	}

	return astar->status;
}

bastar_status_t
bastar_find(bastar_t* astar, const bastar_query_t* query, void* memctx) {
	bastar_begin(astar, query, memctx);
	return bastar_step(astar, SIZE_MAX);
}

void
bastar_edge(bastar_t* astar, bastar_node_t neighbor, bastar_cost_t cost) {
	bastar_record_t* from = astar->current;
	BASTAR_ASSERT(from != NULL, "bastar_edge called outside of expand");

	// Gave up on an earlier edge of this node
	if (astar->status != BASTAR_RUNNING) { return; }
	if (neighbor == from->node) { return; }

	bastar_cost_t g = (bastar_cost_t)(from->g + cost);
	// Also catches NaN and overflow
	BASTAR_ASSERT(g >= from->g, "The cost of an edge must not be negative");

	bastar_record_t** slot = bastar__find_slot(astar, neighbor);
	bastar_record_t* record = *slot;
	if (record == NULL) {
		bastar__discover(astar, slot, neighbor, from, g);
		return;
	}

	if (!(g < record->g)) { return; }

	// g only ever decreases and a record never costs less than its parent,
	// so the parent links cannot form a cycle
	record->g = g;
	record->parent = from;
	if (record->open) {
		bastar__heap_decrease(astar, record);
	} else {
		// Already expanded through a costlier way, which the heuristic made
		// look better at the time. Expand it again.
		bastar__heap_push(astar, record);
	}
	bastar__consider_end(astar, record);
}

// }}}

// Result {{{

// astar->end is only NULL when not even the start could be allocated

bastar_node_t
bastar_end(const bastar_t* astar) {
	return astar->end != NULL ? astar->end->node : astar->query.start;
}

// g is never less than what the parent links add up to.
// It is more only between a node becoming cheaper and that node being
// expanded again, which passes the saving on to the nodes behind it.
bastar_cost_t
bastar_cost(const bastar_t* astar) {
	return astar->end != NULL ? astar->end->g : (bastar_cost_t)0;
}

bastar_path_t
bastar_path(bastar_t* astar) {
	if (astar->path == NULL) {
		size_t len = 0;
		for (
			const bastar_record_t* record = astar->end;
			record != NULL;
			record = record->parent
		) {
			++len;
		}

		bastar_node_t* nodes = BASTAR_ALLOC(
			sizeof(bastar_node_t) * (len > 0 ? len : 1),
			_Alignof(bastar_node_t),
			astar->memctx
		);
		if (nodes == NULL) { return (bastar_path_t){ 0 }; }

		if (len > 0) {
			size_t index = len;
			for (
				const bastar_record_t* record = astar->end;
				record != NULL;
				record = record->parent
			) {
				nodes[--index] = record->node;
			}
		} else {
			nodes[0] = astar->query.start;
			len = 1;
		}

		astar->path = nodes;
		astar->path_len = len;
	}

	return (bastar_path_t){ .nodes = astar->path, .len = astar->path_len };
}

// }}}

#endif
