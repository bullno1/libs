#include "../../bresmon.h"
#include "../../btest.h"
#include <stdio.h>
#include <string.h>
#include <threads.h>

// Created in the working directory and removed after each test
#define TEST_FILE "bresmon_test.txt"

typedef struct {
	bresmon_t* mon;

	int num_reloads;
	const char* last_file;
	void* last_userdata;
} fixture_t;

static fixture_t fixture;

static void
write_file(const char* path, const char* content) {
	FILE* file = fopen(path, "wb");
	BTEST_ASSERT(file != NULL);
	fputs(content, file);
	fclose(file);
}

static void
init_per_test(void) {
	fixture = (fixture_t){ 0 };
	write_file(TEST_FILE, "v1");
	fixture.mon = bresmon_create(NULL);
}

static void
cleanup_per_test(void) {
	bresmon_destroy(fixture.mon);
	remove(TEST_FILE);
}

static btest_suite_t bresmon_ = {
	.name = "bresmon",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

static void
on_reload(const char* file, void* userdata) {
	fixture.num_reloads += 1;
	fixture.last_file = file;
	fixture.last_userdata = userdata;
}

// Poll without blocking so a missing event fails the test instead of
// hanging it
static int
wait_for_events(int timeout_ms) {
	for (int elapsed = 0; elapsed < timeout_ms; elapsed += 10) {
		int num_events = bresmon_should_reload(fixture.mon, false);
		if (num_events > 0) { return num_events; }
		thrd_sleep(&(struct timespec){ .tv_nsec = 10 * 1000 * 1000 }, NULL);
	}

	return 0;
}

BTEST(bresmon_, reload_on_write) {
	static char userdata;
	bresmon_watch_t* watch = bresmon_watch(fixture.mon, TEST_FILE, on_reload, &userdata);
	BTEST_ASSERT(watch != NULL);

	// Nothing happened yet
	BTEST_EXPECT_EQUAL("%d", bresmon_should_reload(fixture.mon, false), 0);
	BTEST_EXPECT_EQUAL("%d", bresmon_reload(fixture.mon), 0);
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 0);

	write_file(TEST_FILE, "v2");
	BTEST_ASSERT(wait_for_events(5000) > 0);

	// Callbacks are only invoked by bresmon_reload
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 0);
	BTEST_EXPECT_EQUAL("%d", bresmon_reload(fixture.mon), 1);
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 1);
	BTEST_ASSERT(fixture.last_file != NULL);
	BTEST_EXPECT(strcmp(fixture.last_file, TEST_FILE) == 0);
	BTEST_EXPECT(fixture.last_userdata == &userdata);

	// A reload is only triggered once per change
	BTEST_EXPECT_EQUAL("%d", bresmon_reload(fixture.mon), 0);
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 1);

	bresmon_unwatch(watch);
}

BTEST(bresmon_, check) {
	bresmon_watch_t* watch = bresmon_watch(fixture.mon, TEST_FILE, on_reload, NULL);
	BTEST_ASSERT(watch != NULL);

	write_file(TEST_FILE, "v2");
	int num_reloads = 0;
	for (int elapsed = 0; elapsed < 5000 && num_reloads == 0; elapsed += 10) {
		num_reloads = bresmon_check(fixture.mon, false);
		thrd_sleep(&(struct timespec){ .tv_nsec = 10 * 1000 * 1000 }, NULL);
	}
	BTEST_EXPECT_EQUAL("%d", num_reloads, 1);
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 1);

	bresmon_unwatch(watch);
}

static void
on_reload2(const char* file, void* userdata) {
	(void)file;
	*(int*)userdata += 1;
}

BTEST(bresmon_, set_watch_callback) {
	bresmon_watch_t* watch = bresmon_watch(fixture.mon, TEST_FILE, on_reload, NULL);
	BTEST_ASSERT(watch != NULL);

	int num_reloads2 = 0;
	bresmon_set_watch_callback(watch, on_reload2, &num_reloads2);

	write_file(TEST_FILE, "v2");
	BTEST_ASSERT(wait_for_events(5000) > 0);
	BTEST_EXPECT_EQUAL("%d", bresmon_reload(fixture.mon), 1);

	// Only the replacement is invoked
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 0);
	BTEST_EXPECT_EQUAL("%d", num_reloads2, 1);

	bresmon_unwatch(watch);
}

BTEST(bresmon_, unwatch) {
	bresmon_watch_t* watch = bresmon_watch(fixture.mon, TEST_FILE, on_reload, NULL);
	BTEST_ASSERT(watch != NULL);
	bresmon_unwatch(watch);

	write_file(TEST_FILE, "v2");
	BTEST_EXPECT_EQUAL("%d", wait_for_events(200), 0);
	BTEST_EXPECT_EQUAL("%d", bresmon_reload(fixture.mon), 0);
	BTEST_EXPECT_EQUAL("%d", fixture.num_reloads, 0);
}

#define BLIB_IMPLEMENTATION
#include "../../bresmon.h"
