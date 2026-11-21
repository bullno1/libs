#include "../../barena.h"
#include "../../bastar.h"
#include "../../btest.h"

#include <stdlib.h>
#include <string.h>

// Fixture {{{

// Every allocation of bastar goes through test_alloc (see the end of this
// file), so that tests can make the allocator fail.
typedef struct {
	barena_t arena;
	size_t num_allocs;
	/// Allocations that succeed before every later one fails
	size_t budget;
} test_mem_t;

static struct {
	barena_pool_t pool;
	test_mem_t mem;
} fixture;

static void*
test_alloc(size_t size, size_t align, void* ctx) {
	test_mem_t* mem = ctx;
	if (mem->num_allocs >= mem->budget) { return NULL; }
	++mem->num_allocs;
	return barena_memalign(&mem->arena, size, align);
}

static void
init_per_suite(void) {
	barena_pool_init(&fixture.pool, 64 * 1024);
}

static void
cleanup_per_suite(void) {
	barena_pool_cleanup(&fixture.pool);
}

static void
init_per_test(void) {
	barena_init(&fixture.mem.arena, &fixture.pool);
	fixture.mem.num_allocs = 0;
	fixture.mem.budget = SIZE_MAX;
}

static void
cleanup_per_test(void) {
	barena_reset(&fixture.mem.arena);
}

static btest_suite_t pathfinding = {
	.name = "bastar",
	.init_per_suite = init_per_suite,
	.cleanup_per_suite = cleanup_per_suite,
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

static uint32_t rng_state;

static uint32_t
rng_next(void) {
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

// }}}

// Grid world {{{

#define GRID_MAX 32

// 4-connected, every step costs 1
typedef struct {
	int w, h;
	bool walls[GRID_MAX * GRID_MAX];
	int goal_x, goal_y;
	/// Reaching any of these also ends the search
	bool ore[GRID_MAX * GRID_MAX];
} grid_t;

static inline bastar_node_t
grid_node(int x, int y) {
	return (uint64_t)(uint32_t)x << 32 | (uint32_t)y;
}

static inline int
grid_x(bastar_node_t node) { return (int)(node >> 32); }

static inline int
grid_y(bastar_node_t node) { return (int)(uint32_t)node; }

static bool
grid_is_walkable(const grid_t* grid, int x, int y) {
	return x >= 0 && y >= 0 && x < grid->w && y < grid->h
		&& !grid->walls[y * grid->w + x];
}

// Rows of '.', '#' for a wall, 'S' for the start, 'G' for the goal and 'o'
// for ore
static bastar_node_t
grid_parse(grid_t* grid, int w, int h, const char* map) {
	*grid = (grid_t){ .w = w, .h = h, .goal_x = -1, .goal_y = -1 };
	bastar_node_t start = grid_node(0, 0);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			char cell = map[y * w + x];
			grid->walls[y * w + x] = cell == '#';
			grid->ore[y * w + x] = cell == 'o';
			if (cell == 'S') { start = grid_node(x, y); }
			if (cell == 'G') { grid->goal_x = x; grid->goal_y = y; }
		}
	}
	return start;
}

static void
grid_expand(void* userdata, bastar_t* astar, bastar_node_t node) {
	const grid_t* grid = userdata;
	static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	for (int i = 0; i < 4; ++i) {
		int x = grid_x(node) + dirs[i][0];
		int y = grid_y(node) + dirs[i][1];
		if (grid_is_walkable(grid, x, y)) {
			bastar_edge(astar, grid_node(x, y), 1.f);
		}
	}
}

static bastar_cost_t
grid_manhattan(void* userdata, bastar_node_t node) {
	const grid_t* grid = userdata;
	return (bastar_cost_t)(
		abs(grid_x(node) - grid->goal_x) + abs(grid_y(node) - grid->goal_y)
	);
}

static bool
grid_at_goal(void* userdata, bastar_node_t node) {
	const grid_t* grid = userdata;
	return node == grid_node(grid->goal_x, grid->goal_y);
}

static bool
grid_at_ore(void* userdata, bastar_node_t node) {
	const grid_t* grid = userdata;
	return grid->ore[grid_y(node) * grid->w + grid_x(node)];
}

static bastar_query_t
grid_query(grid_t* grid, bastar_node_t start) {
	return (bastar_query_t){
		.expand = grid_expand,
		.heuristic = grid_manhattan,
		.is_goal = grid_at_goal,
		.userdata = grid,
		.start = start,
	};
}

// Steps from the start to every cell by breadth-first search, -1 when
// unreachable. Returns the number of reachable cells.
static int
grid_bfs(const grid_t* grid, bastar_node_t start, int* distances) {
	int queue[GRID_MAX * GRID_MAX];
	int head = 0, tail = 0;
	for (int i = 0; i < grid->w * grid->h; ++i) { distances[i] = -1; }

	int start_index = grid_y(start) * grid->w + grid_x(start);
	distances[start_index] = 0;
	queue[tail++] = start_index;
	while (head < tail) {
		int index = queue[head++];
		static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (int i = 0; i < 4; ++i) {
			int x = index % grid->w + dirs[i][0];
			int y = index / grid->w + dirs[i][1];
			if (!grid_is_walkable(grid, x, y)) { continue; }
			if (distances[y * grid->w + x] >= 0) { continue; }
			distances[y * grid->w + x] = distances[index] + 1;
			queue[tail++] = y * grid->w + x;
		}
	}
	return tail;
}

// The path is a walk from the start to bastar_end that costs bastar_cost
static void
grid_check_path(const grid_t* grid, bastar_t* astar, bastar_node_t start) {
	bastar_path_t path = bastar_path(astar);
	BTEST_ASSERT(path.nodes != NULL);
	BTEST_ASSERT(path.len >= 1);
	BTEST_EXPECT(path.nodes[0] == start);
	BTEST_EXPECT(path.nodes[path.len - 1] == bastar_end(astar));
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(astar), (double)(path.len - 1));

	for (size_t i = 0; i < path.len; ++i) {
		bastar_node_t node = path.nodes[i];
		BTEST_ASSERT(grid_is_walkable(grid, grid_x(node), grid_y(node)));
		if (i == 0) { continue; }
		bastar_node_t prev = path.nodes[i - 1];
		int step = abs(grid_x(node) - grid_x(prev)) + abs(grid_y(node) - grid_y(prev));
		BTEST_ASSERT_EQUAL("%d", step, 1);
	}
}

// }}}

// Grid searches {{{

BTEST(pathfinding, straight_line) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 8, 1, "S......G");
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT_EQUAL("%d", (int)astar.status, (int)BASTAR_FOUND);
	BTEST_EXPECT(bastar_end(&astar) == grid_node(7, 0));
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 7.0);

	bastar_path_t path = bastar_path(&astar);
	BTEST_ASSERT_EQUAL("%zu", path.len, (size_t)8);
	for (int x = 0; x < 8; ++x) {
		BTEST_EXPECT(path.nodes[x] == grid_node(x, 0));
	}

	// A perfect heuristic never leaves the path
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)8);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)7);
}

// On an open grid every node on the way has the same estimated total.
// Preferring the one farthest from the start walks straight to the goal
// instead of flooding the whole rectangle.
BTEST(pathfinding, ties_go_deeper) {
	grid_t grid = { .w = 16, .h = 16, .goal_x = 15, .goal_y = 15 };
	bastar_node_t start = grid_node(0, 0);
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 30.0);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)30);
	grid_check_path(&grid, &astar, start);
}

BTEST(pathfinding, around_walls) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 8, 5,
		"S...#..."
		".##.#.#."
		".#..#.#."
		".#.##.#G"
		".#....#."
	);
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT(bastar_end(&astar) == grid_node(7, 3));
	grid_check_path(&grid, &astar, start);

	int distances[GRID_MAX * GRID_MAX];
	grid_bfs(&grid, start, distances);
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), (double)distances[3 * 8 + 7]);
}

BTEST(pathfinding, start_is_goal) {
	grid_t grid;
	grid_parse(&grid, 3, 1, "..G");
	bastar_query_t query = grid_query(&grid, grid_node(2, 0));

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)0);
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 0.0);

	bastar_path_t path = bastar_path(&astar);
	BTEST_ASSERT_EQUAL("%zu", path.len, (size_t)1);
	BTEST_EXPECT(path.nodes[0] == grid_node(2, 0));
}

BTEST(pathfinding, unreachable_leads_to_the_closest_node) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 8, 3,
		"S....#.."
		".....#.G"
		".....#.."
	);
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_UNREACHABLE);
	// Everything on this side of the wall was discovered
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)15);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)15);

	// Right against the wall, on the row of the goal
	BTEST_EXPECT(bastar_end(&astar) == grid_node(4, 1));
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 5.0);
	grid_check_path(&grid, &astar, start);

	// A finished search stays finished
	status = bastar_step(&astar, 100);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_UNREACHABLE);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)15);
}

BTEST(pathfinding, max_nodes) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 8, 4,
		"S......."
		"........"
		"........"
		".......G"
	);

	// No limit: the goal is found
	bastar_query_t query = grid_query(&grid, start);
	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	size_t needed = astar.num_nodes;
	BTEST_ASSERT(needed > 6);

	// Exactly enough
	query.max_nodes = needed;
	status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, needed);

	// One short
	for (size_t max_nodes = 1; max_nodes < needed; ++max_nodes) {
		query.max_nodes = max_nodes;
		size_t allocs_before = fixture.mem.num_allocs;
		status = bastar_find(&astar, &query, &fixture.mem);
		BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_GAVE_UP);
		BTEST_ASSERT_EQUAL("%zu", astar.num_nodes, max_nodes);
		BTEST_ASSERT_EQUAL("%zu", fixture.mem.num_allocs - allocs_before, max_nodes);

		// Still a walk towards the goal
		grid_check_path(&grid, &astar, start);
		status = bastar_step(&astar, 100);
		BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_GAVE_UP);
	}
}

BTEST(pathfinding, no_goal_explores_everything) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 6, 3,
		"..#..."
		".S#.#."
		"..#.#."
	);
	bastar_query_t query = { .expand = grid_expand, .userdata = &grid, .start = start };

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_UNREACHABLE);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)6);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)6);

	// Without an estimate, no node is closer than the start
	BTEST_EXPECT(bastar_end(&astar) == start);
	BTEST_EXPECT_EQUAL("%zu", bastar_path(&astar).len, (size_t)1);
}

BTEST(pathfinding, nearest_of_many) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 9, 3,
		".o...#o.."
		"....S#..."
		"........."
	);
	// The ore on the right is closer as the crow flies, but behind a wall
	bastar_query_t query = {
		.expand = grid_expand,
		.is_goal = grid_at_ore,
		.userdata = &grid,
		.start = start,
	};

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT(bastar_end(&astar) == grid_node(1, 0));
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 4.0);
	grid_check_path(&grid, &astar, start);
}

static void
grid_randomize(grid_t* grid, bastar_node_t* start) {
	*grid = (grid_t){
		.w = 2 + (int)(rng_next() % (GRID_MAX - 1)),
		.h = 2 + (int)(rng_next() % (GRID_MAX - 1)),
	};
	uint32_t density = rng_next() % 45;
	for (int i = 0; i < grid->w * grid->h; ++i) {
		grid->walls[i] = rng_next() % 100 < density;
	}

	int start_x = (int)(rng_next() % (uint32_t)grid->w);
	int start_y = (int)(rng_next() % (uint32_t)grid->h);
	grid->goal_x = (int)(rng_next() % (uint32_t)grid->w);
	grid->goal_y = (int)(rng_next() % (uint32_t)grid->h);
	grid->walls[start_y * grid->w + start_x] = false;
	grid->walls[grid->goal_y * grid->w + grid->goal_x] = false;
	*start = grid_node(start_x, start_y);
}

BTEST(pathfinding, random_grids_against_bfs) {
	rng_state = 0x1234567;
	int num_found = 0;
	int num_unreachable = 0;

	for (int round = 0; round < 500; ++round) {
		grid_t grid;
		bastar_node_t start;
		grid_randomize(&grid, &start);

		int distances[GRID_MAX * GRID_MAX];
		int num_reachable = grid_bfs(&grid, start, distances);
		int expected = distances[grid.goal_y * grid.w + grid.goal_x];

		// With and without a heuristic
		for (int use_heuristic = 0; use_heuristic < 2; ++use_heuristic) {
			bastar_query_t query = grid_query(&grid, start);
			if (!use_heuristic) { query.heuristic = NULL; }

			barena_snapshot_t snapshot = barena_snapshot(&fixture.mem.arena);
			bastar_t astar;
			bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
			if (expected >= 0) {
				BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
				BTEST_ASSERT_EQUAL("%f", (double)bastar_cost(&astar), (double)expected);
				BTEST_ASSERT(bastar_end(&astar) == grid_node(grid.goal_x, grid.goal_y));
				++num_found;
			} else {
				BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_UNREACHABLE);
				BTEST_ASSERT_EQUAL("%zu", astar.num_nodes, (size_t)num_reachable);
				++num_unreachable;
			}
			BTEST_ASSERT(astar.num_nodes <= (size_t)num_reachable);
			// A consistent heuristic never expands a node twice
			BTEST_ASSERT(astar.num_expansions <= astar.num_nodes);
			grid_check_path(&grid, &astar, start);
			barena_restore(&fixture.mem.arena, snapshot);
		}
	}

	// Both outcomes were exercised
	BTEST_EXPECT(num_found > 100);
	BTEST_EXPECT(num_unreachable > 100);
}

static bool
same_result(bastar_t* lhs, bastar_t* rhs) {
	bastar_path_t lhs_path = bastar_path(lhs);
	bastar_path_t rhs_path = bastar_path(rhs);
	return lhs->status == rhs->status
		&& lhs->num_nodes == rhs->num_nodes
		&& lhs->num_expansions == rhs->num_expansions
		&& bastar_end(lhs) == bastar_end(rhs)
		&& bastar_cost(lhs) == bastar_cost(rhs)
		&& lhs_path.len == rhs_path.len
		&& memcmp(lhs_path.nodes, rhs_path.nodes, sizeof(bastar_node_t) * lhs_path.len) == 0;
}

BTEST(pathfinding, sliced_search_matches_whole_search) {
	rng_state = 0xbeef;

	for (int round = 0; round < 100; ++round) {
		grid_t grid;
		bastar_node_t start;
		grid_randomize(&grid, &start);
		bastar_query_t query = grid_query(&grid, start);

		bastar_t whole;
		bastar_find(&whole, &query, &fixture.mem);

		// Records land on other addresses this time
		test_alloc(1 + rng_next() % 200, 1, &fixture.mem);

		size_t budget = 1 + rng_next() % 5;
		bastar_t sliced;
		bastar_status_t status = bastar_begin(&sliced, &query, &fixture.mem);
		size_t num_steps = 0;
		while (status == BASTAR_RUNNING) {
			size_t expansions_before = sliced.num_expansions;
			status = bastar_step(&sliced, budget);
			BTEST_ASSERT(sliced.num_expansions - expansions_before <= budget);
			if (status == BASTAR_RUNNING) {
				BTEST_ASSERT_EQUAL("%zu", sliced.num_expansions - expansions_before, budget);
				// The best so far can be walked while waiting
				grid_check_path(&grid, &sliced, start);
			}
			++num_steps;
		}
		BTEST_ASSERT(num_steps >= whole.num_expansions / budget);
		BTEST_ASSERT(same_result(&whole, &sliced));

		barena_reset(&fixture.mem.arena);
	}
}

BTEST(pathfinding, zero_budget_expands_nothing) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 3, 1, "S.G");
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_status_t status = bastar_begin(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_RUNNING);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)1);
	status = bastar_step(&astar, 0);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_RUNNING);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)0);

	status = bastar_step(&astar, 2);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)2);
}

// }}}

// Memory {{{

BTEST(pathfinding, path_is_allocated_once) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 8, 1, "S......G");
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_begin(&astar, &query, &fixture.mem);
	bastar_step(&astar, 3);
	BTEST_ASSERT_EQUAL("%d", (int)astar.status, (int)BASTAR_RUNNING);

	// While running: the best so far
	size_t num_allocs = fixture.mem.num_allocs;
	bastar_path_t partial = bastar_path(&astar);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem.num_allocs, num_allocs + 1);
	BTEST_EXPECT_EQUAL("%zu", partial.len, (size_t)4);
	BTEST_EXPECT(bastar_path(&astar).nodes == partial.nodes);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem.num_allocs, num_allocs + 1);

	// A step makes it stale
	bastar_step(&astar, SIZE_MAX);
	BTEST_ASSERT_EQUAL("%d", (int)astar.status, (int)BASTAR_FOUND);
	num_allocs = fixture.mem.num_allocs;
	bastar_path_t path = bastar_path(&astar);
	BTEST_EXPECT(path.nodes != partial.nodes);
	BTEST_EXPECT_EQUAL("%zu", path.len, (size_t)8);
	BTEST_EXPECT_EQUAL("%zu", partial.len, (size_t)4);  // Untouched

	// Once finished, it never changes again
	bastar_step(&astar, SIZE_MAX);
	BTEST_EXPECT(bastar_path(&astar).nodes == path.nodes);
	BTEST_EXPECT_EQUAL("%zu", fixture.mem.num_allocs, num_allocs + 1);
}

BTEST(pathfinding, allocation_failure) {
	grid_t grid;
	bastar_node_t start = grid_parse(&grid, 8, 4,
		"S......."
		"....#..."
		"....#..."
		"....#..G"
	);
	bastar_query_t query = grid_query(&grid, start);

	bastar_t astar;
	bastar_find(&astar, &query, &fixture.mem);
	BTEST_ASSERT_EQUAL("%d", (int)astar.status, (int)BASTAR_FOUND);
	size_t needed = astar.num_nodes;

	// Not even the start
	barena_reset(&fixture.mem.arena);
	fixture.mem.num_allocs = 0;
	fixture.mem.budget = 0;
	bastar_status_t status = bastar_begin(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_GAVE_UP);
	status = bastar_step(&astar, 10);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_GAVE_UP);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)0);
	BTEST_EXPECT(bastar_end(&astar) == start);
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 0.0);
	BTEST_EXPECT(bastar_path(&astar).nodes == NULL);
	BTEST_EXPECT_EQUAL("%zu", bastar_path(&astar).len, (size_t)0);
	// Going nowhere is still a path
	fixture.mem.budget = 1;
	BTEST_EXPECT_EQUAL("%zu", bastar_path(&astar).len, (size_t)1);
	BTEST_EXPECT(bastar_path(&astar).nodes[0] == start);

	// Running out at every possible point
	for (size_t budget = 1; budget < needed; ++budget) {
		barena_reset(&fixture.mem.arena);
		fixture.mem.num_allocs = 0;
		fixture.mem.budget = budget;

		status = bastar_find(&astar, &query, &fixture.mem);
		BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_GAVE_UP);
		BTEST_ASSERT_EQUAL("%zu", astar.num_nodes, budget);

		// No room for the path either
		bastar_path_t path = bastar_path(&astar);
		BTEST_ASSERT(path.nodes == NULL);
		BTEST_ASSERT_EQUAL("%zu", path.len, (size_t)0);

		// It was not remembered as a failure
		fixture.mem.budget = budget + 1;
		grid_check_path(&grid, &astar, start);
	}
}

// }}}

// Weighted graphs {{{

#define GRAPH_MAX 48
#define GRAPH_NO_EDGE (-1.f)

// Dense and directed
typedef struct {
	int num_nodes;
	float costs[GRAPH_MAX][GRAPH_MAX];
	float estimates[GRAPH_MAX];
	int goal;
	/// Report every edge this many times, the real cost last
	int num_duplicates;
	/// How many times each node was expanded
	int num_expansions[GRAPH_MAX];
} graph_t;

// Names that do not look like small integers
static inline bastar_node_t
graph_node(int index) { return (uint64_t)index << 40 | UINT64_C(0xdeadbeef); }

static inline int
graph_index(bastar_node_t node) { return (int)(node >> 40); }

static void
graph_expand(void* userdata, bastar_t* astar, bastar_node_t node) {
	graph_t* graph = userdata;
	int from = graph_index(node);
	++graph->num_expansions[from];
	for (int to = 0; to < graph->num_nodes; ++to) {
		float cost = graph->costs[from][to];
		if (cost == GRAPH_NO_EDGE) { continue; }
		for (int i = graph->num_duplicates; i > 0; --i) {
			bastar_edge(astar, graph_node(to), cost + (float)i);
		}
		bastar_edge(astar, graph_node(to), cost);
	}
}

static bastar_cost_t
graph_estimate(void* userdata, bastar_node_t node) {
	const graph_t* graph = userdata;
	return graph->estimates[graph_index(node)];
}

static bool
graph_at_goal(void* userdata, bastar_node_t node) {
	const graph_t* graph = userdata;
	return graph_index(node) == graph->goal;
}

// The true cost from every node to the goal, by Bellman-Ford.
// Infinity is a cost no path can reach.
#define GRAPH_INFINITY 1e9f

static void
graph_solve(const graph_t* graph, float* to_goal) {
	for (int i = 0; i < graph->num_nodes; ++i) { to_goal[i] = GRAPH_INFINITY; }
	to_goal[graph->goal] = 0.f;
	for (int round = 0; round < graph->num_nodes; ++round) {
		for (int from = 0; from < graph->num_nodes; ++from) {
			for (int to = 0; to < graph->num_nodes; ++to) {
				float cost = graph->costs[from][to];
				if (cost == GRAPH_NO_EDGE || to_goal[to] == GRAPH_INFINITY) { continue; }
				if (to_goal[to] + cost < to_goal[from]) { to_goal[from] = to_goal[to] + cost; }
			}
		}
	}
}

BTEST(pathfinding, edges) {
	// 0 -> 1 is listed with a self loop, 1 -> 2 is free, 0 -> 2 is a detour
	graph_t graph = { .num_nodes = 3, .goal = 2, .num_duplicates = 3 };
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) { graph.costs[i][j] = GRAPH_NO_EDGE; }
	}
	graph.costs[0][0] = 0.f;
	graph.costs[0][1] = 2.f;
	graph.costs[0][2] = 3.f;
	graph.costs[1][1] = 0.f;
	graph.costs[1][2] = 0.f;
	graph.costs[1][0] = 0.f;

	bastar_query_t query = {
		.expand = graph_expand,
		.is_goal = graph_at_goal,
		.userdata = &graph,
		.start = graph_node(0),
	};
	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	// The cheapest of the duplicates, and through the free edge
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), 2.0);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)3);

	bastar_path_t path = bastar_path(&astar);
	BTEST_ASSERT_EQUAL("%zu", path.len, (size_t)3);
	BTEST_EXPECT(path.nodes[0] == graph_node(0));
	BTEST_EXPECT(path.nodes[1] == graph_node(1));
	BTEST_EXPECT(path.nodes[2] == graph_node(2));
}

// An estimate that never overestimates but is not consistent makes the
// search expand nodes through a costlier way first, then again.
BTEST(pathfinding, random_graphs_with_inconsistent_heuristic) {
	rng_state = 0xa57a2;
	int num_found = 0;
	int num_unreachable = 0;
	int num_reopened = 0;

	static graph_t graph;
	for (int round = 0; round < 400; ++round) {
		graph = (graph_t){
			.num_nodes = 2 + (int)(rng_next() % (GRAPH_MAX - 1)),
			.num_duplicates = (int)(rng_next() % 2),
		};
		graph.goal = (int)(rng_next() % (uint32_t)graph.num_nodes);
		int start = (int)(rng_next() % (uint32_t)graph.num_nodes);
		uint32_t density = 2 + rng_next() % 30;
		for (int from = 0; from < graph.num_nodes; ++from) {
			for (int to = 0; to < graph.num_nodes; ++to) {
				// Small integers: every sum is exact. 0 is a legal cost.
				graph.costs[from][to] = rng_next() % 100 < density
					? (float)(rng_next() % 20)
					: GRAPH_NO_EDGE;
			}
		}

		float to_goal[GRAPH_MAX];
		graph_solve(&graph, to_goal);
		for (int i = 0; i < graph.num_nodes; ++i) {
			// Either the truth or nothing, independently for each node: as
			// inconsistent as it gets without overestimating
			float truth = to_goal[i] == GRAPH_INFINITY ? 100.f : to_goal[i];
			graph.estimates[i] = rng_next() % 2 == 0 ? truth : 0.f;
		}

		bastar_query_t query = {
			.expand = graph_expand,
			.heuristic = graph_estimate,
			.is_goal = graph_at_goal,
			.userdata = &graph,
			.start = graph_node(start),
		};
		bastar_t astar;
		bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
		for (int i = 0; i < graph.num_nodes; ++i) {
			if (graph.num_expansions[i] > 1) {
				++num_reopened;
				break;
			}
		}

		// The path is made of real edges
		bastar_path_t path = bastar_path(&astar);
		BTEST_ASSERT(path.nodes != NULL);
		BTEST_ASSERT(path.nodes[0] == graph_node(start));
		BTEST_ASSERT(path.nodes[path.len - 1] == bastar_end(&astar));
		float path_cost = 0.f;
		for (size_t i = 1; i < path.len; ++i) {
			float cost = graph.costs[graph_index(path.nodes[i - 1])][graph_index(path.nodes[i])];
			BTEST_ASSERT(cost != GRAPH_NO_EDGE);
			path_cost += cost;
		}

		if (to_goal[start] != GRAPH_INFINITY) {
			BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
			BTEST_ASSERT(graph_index(bastar_end(&astar)) == graph.goal);
			BTEST_ASSERT_EQUAL("%f", (double)bastar_cost(&astar), (double)to_goal[start]);
			BTEST_ASSERT_EQUAL("%f", (double)path_cost, (double)to_goal[start]);
			++num_found;
		} else {
			BTEST_ASSERT_EQUAL("%d", (int)status, (int)BASTAR_UNREACHABLE);
			// Nothing is left to expand again, so every cost is settled
			BTEST_ASSERT_EQUAL("%f", (double)path_cost, (double)bastar_cost(&astar));
			++num_unreachable;
		}

		barena_reset(&fixture.mem.arena);
	}

	// Every interesting case was exercised
	BTEST_EXPECT(num_found > 50);
	BTEST_EXPECT(num_unreachable > 50);
	// Only some graphs have a node that is expanded too early
	BTEST_EXPECT(num_reopened > 20);
}

// }}}

// Scale {{{

#define FAN_SIZE UINT64_C(200000)

// One hub with an edge to every other node, costlier ones first, and nothing
// else. The open set is as wide as it gets.
static void
fan_expand(void* userdata, bastar_t* astar, bastar_node_t node) {
	(void)userdata;
	if (node != 0) { return; }
	for (uint64_t i = FAN_SIZE; i > 0; --i) {
		bastar_edge(astar, i, (float)i);
	}
}

static bool
fan_is_last(void* userdata, bastar_node_t node) {
	size_t* num_calls = userdata;
	++*num_calls;
	return node == FAN_SIZE;
}

BTEST(pathfinding, wide_fan) {
	size_t num_goal_tests = 0;
	bastar_query_t query = {
		.expand = fan_expand,
		.is_goal = fan_is_last,
		.userdata = &num_goal_tests,
		.start = 0,
	};

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_FOUND);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)FAN_SIZE + 1);
	// The costliest node comes out last
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)FAN_SIZE);
	BTEST_EXPECT_EQUAL("%zu", num_goal_tests, (size_t)FAN_SIZE + 1);
	BTEST_EXPECT_EQUAL("%f", (double)bastar_cost(&astar), (double)FAN_SIZE);
	BTEST_EXPECT_EQUAL("%zu", bastar_path(&astar).len, (size_t)2);
}

#define CHAIN_SIZE UINT64_C(200000)

// Every node leads to the next and back to the first
static void
chain_expand(void* userdata, bastar_t* astar, bastar_node_t node) {
	(void)userdata;
	bastar_edge(astar, 0, 1.f);
	if (node + 1 < CHAIN_SIZE) { bastar_edge(astar, node + 1, 1.f); }
}

BTEST(pathfinding, long_chain) {
	bastar_query_t query = { .expand = chain_expand, .start = 0 };

	bastar_t astar;
	bastar_status_t status = bastar_find(&astar, &query, &fixture.mem);
	BTEST_EXPECT_EQUAL("%d", (int)status, (int)BASTAR_UNREACHABLE);
	BTEST_EXPECT_EQUAL("%zu", astar.num_nodes, (size_t)CHAIN_SIZE);
	BTEST_EXPECT_EQUAL("%zu", astar.num_expansions, (size_t)CHAIN_SIZE);
}

// }}}

// barena is implemented by tests/barena/main.c
#define BASTAR_ALLOC(size, align, ctx) test_alloc(size, align, ctx)
#define BLIB_IMPLEMENTATION
#include "../../bastar.h"
