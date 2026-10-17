// The log must bring the state back as it was, drop what was not whole, never bury a torn
// tail, and survive a crash at any point of a compaction. The file system is in memory
// and can cut a file, limit a transfer, or fail on demand.

#include "../../bwal.h"
#include "../../btest.h"
#include <stdlib.h>
#include <string.h>

// In-memory file system {{{

typedef struct {
	uint8_t* data;
	size_t len;
	size_t cap;
	bool exists;
} mem_file_t;

typedef struct {
	mem_file_t files[2];
	// Faults. SIZE_MAX means never.
	size_t write_budget;      // bytes accepted before every write fails
	size_t max_io;            // largest transfer per call, to exercise the loops
	size_t read_fail_after;   // successful read calls before every read fails
	bool refuse_append;       // opening for append fails, as a read only medium would
	int num_open;             // handles out, to catch leaks
} mem_fs_t;

struct bwal_file_s {
	mem_fs_t* fs;
	mem_file_t* file;
	size_t pos;
};

static size_t
mem_cap(mem_fs_t* fs, size_t len) {
	return fs->max_io != SIZE_MAX && fs->max_io < len ? fs->max_io : len;
}

static bwal_file_t*
mem_open(void* userdata, const char* path, bwal_open_mode_t mode) {
	mem_fs_t* fs = userdata;
	mem_file_t* file = &fs->files[path[0] - '0'];
	if (mode == BWAL_OPEN_READ && !file->exists) { return NULL; }
	if (mode == BWAL_OPEN_APPEND && fs->refuse_append) { return NULL; }
	if (mode == BWAL_OPEN_TRUNCATE) { file->len = 0; }
	file->exists = true;

	bwal_file_t* handle = malloc(sizeof(*handle));
	*handle = (bwal_file_t){ .fs = fs, .file = file, .pos = mode == BWAL_OPEN_APPEND ? file->len : 0 };
	++fs->num_open;
	return handle;
}

static int64_t
mem_read(bwal_file_t* handle, void* buf, size_t len) {
	mem_fs_t* fs = handle->fs;
	if (fs->read_fail_after == 0) { return -1; }
	if (fs->read_fail_after != SIZE_MAX) { --fs->read_fail_after; }

	size_t available = handle->file->len - handle->pos;
	size_t n = mem_cap(fs, len < available ? len : available);
	memcpy(buf, handle->file->data + handle->pos, n);
	handle->pos += n;
	return (int64_t)n;
}

static size_t
mem_write(bwal_file_t* handle, const void* buf, size_t len) {
	mem_fs_t* fs = handle->fs;
	if (fs->write_budget == 0) { return 0; }
	size_t n = mem_cap(fs, len);
	if (fs->write_budget != SIZE_MAX) {
		if (n > fs->write_budget) { n = fs->write_budget; }
		fs->write_budget -= n;
	}

	mem_file_t* file = handle->file;
	size_t end = handle->pos + n;
	if (end > file->cap) {
		file->cap = end * 2;
		file->data = realloc(file->data, file->cap);
	}
	memcpy(file->data + handle->pos, buf, n);
	handle->pos = end;
	file->len = end;
	return n;
}

static void
mem_close(bwal_file_t* handle) {
	--handle->fs->num_open;
	free(handle);
}

static const bwal_io_t mem_io_template = {
	.open = mem_open,
	.read = mem_read,
	.write = mem_write,
	.close = mem_close,
};

static void
mem_reset(mem_fs_t* fs) {
	for (int i = 0; i < 2; ++i) { free(fs->files[i].data); }
	*fs = (mem_fs_t){ .write_budget = SIZE_MAX, .max_io = SIZE_MAX, .read_fail_after = SIZE_MAX };
}

static void
mem_copy(mem_fs_t* dst, const mem_fs_t* src) {
	mem_reset(dst);
	for (int i = 0; i < 2; ++i) {
		const mem_file_t* from = &src->files[i];
		mem_file_t* to = &dst->files[i];
		*to = (mem_file_t){ .len = from->len, .cap = from->len, .exists = from->exists };
		if (from->len > 0) {
			to->data = malloc(from->len);
			memcpy(to->data, from->data, from->len);
		}
	}
}

// }}}

// The state: a list of numbers. A snapshot carries all of them, an entry one or more.
// Payload: kind (0 snapshot, 1 entry), count, then the values as little endian int32.
// Self delimiting, as the format requires.
// {{{

#define MAX_VALUES 64

typedef struct {
	int values[MAX_VALUES];
	int num_values;

	// What prepare read, applied by commit
	int staged[MAX_VALUES];
	int num_staged;
	bool staged_snapshot;
	// The first entry of a slot is its snapshot; so is what follows a rejected one. A
	// commit is what moves past it.
	bool expect_snapshot;

	int num_prepares;
	int num_commits;
	int num_rollbacks;

	// Faults
	int refuse_at;    // the prepare call that returns BWAL_ERR_INVALID, -1 for none
	bool overread;    // prepare reads one byte past the payload
} model_t;

static void
put_i32(uint8_t* out, int value) {
	uint32_t u = (uint32_t)value;
	out[0] = (uint8_t)(u & 0xFFu);
	out[1] = (uint8_t)((u >> 8) & 0xFFu);
	out[2] = (uint8_t)((u >> 16) & 0xFFu);
	out[3] = (uint8_t)((u >> 24) & 0xFFu);
}

static int
get_i32(const uint8_t* in) {
	return (int)((uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24));
}

static bwal_err_t
model_prepare(void* ctx, bwal_t* wal) {
	model_t* model = ctx;
	int call = model->num_prepares++;
	if (call == model->refuse_at) { return BWAL_ERR_INVALID; }

	uint8_t head[2];
	if (bwal_replay_read(wal, head, sizeof(head)) != sizeof(head)) { return BWAL_OK; }
	bool snapshot = head[0] == 0;
	if (head[0] > 1 || snapshot != model->expect_snapshot || head[1] > MAX_VALUES) { return BWAL_ERR_INVALID; }

	model->staged_snapshot = snapshot;
	model->num_staged = head[1];
	for (int i = 0; i < model->num_staged; ++i) {
		uint8_t bytes[4];
		if (bwal_replay_read(wal, bytes, sizeof(bytes)) != sizeof(bytes)) { return BWAL_OK; }
		model->staged[i] = get_i32(bytes);
	}
	if (model->overread) {
		uint8_t extra;
		bwal_replay_read(wal, &extra, 1);
	}
	return BWAL_OK;
}

static void
model_commit(void* ctx) {
	model_t* model = ctx;
	++model->num_commits;
	if (model->staged_snapshot) {
		memcpy(model->values, model->staged, sizeof(model->staged));
		model->num_values = model->num_staged;
	} else {
		for (int i = 0; i < model->num_staged && model->num_values < MAX_VALUES; ++i) {
			model->values[model->num_values++] = model->staged[i];
		}
	}
	model->num_staged = 0;
	model->expect_snapshot = false;
}

static void
model_rollback(void* ctx) {
	model_t* model = ctx;
	++model->num_rollbacks;
	model->num_staged = 0;
	// A snapshot that did not verify is followed by another slot's: start from nothing
	if (model->expect_snapshot) { model->num_values = 0; }
}

static bwal_err_t
model_write_payload(bwal_t* wal, bwal_err_t (*write)(bwal_t*, const void*, size_t), bool snapshot, const int* values, int count) {
	uint8_t head[2] = { snapshot ? 0 : 1, (uint8_t)count };
	bwal_err_t err = write(wal, head, sizeof(head));
	for (int i = 0; err == BWAL_OK && i < count; ++i) {
		uint8_t bytes[4];
		put_i32(bytes, values[i]);
		err = write(wal, bytes, sizeof(bytes));
	}
	return err;
}

static bwal_err_t
model_compact(model_t* model, bwal_t* wal) {
	bwal_err_t err = bwal_compact_begin(wal);
	if (err != BWAL_OK) { return err; }
	err = model_write_payload(wal, bwal_compact_write, true, model->values, model->num_values);
	if (err != BWAL_OK) { return err; }
	return bwal_compact_end(wal);
}

// The value goes into memory first, as a game applies a tick before logging it
static bwal_err_t
model_append(model_t* model, bwal_t* wal, int value) {
	model->values[model->num_values++] = value;
	bwal_err_t err = bwal_entry_begin(wal);
	if (err != BWAL_OK) { return err; }
	err = model_write_payload(wal, bwal_entry_write, false, &value, 1);
	if (err != BWAL_OK) { return err; }
	return bwal_entry_commit(wal);
}

static bool
model_equals(const model_t* model, const int* values, int count) {
	return model->num_values == count && memcmp(model->values, values, sizeof(int) * (size_t)count) == 0;
}

// }}}

// Fixture {{{

#define HEADER_LEN 6
#define ENTRY_OVERHEAD 12   // magic and trailer
#define PAYLOAD_LEN(count) (2 + 4 * (size_t)(count))

typedef struct {
	mem_fs_t fs;
	bwal_io_t io;
	model_t model;
	bwal_t wal;
} fixture_t;

static fixture_t fixture;

static void
init_per_test(void) {
	memset(&fixture, 0, sizeof(fixture));
	mem_reset(&fixture.fs);
	fixture.io = mem_io_template;
	fixture.io.userdata = &fixture.fs;
	fixture.model.refuse_at = -1;
}

static void
cleanup_per_test(void) {
	bwal_close(&fixture.wal);
	BTEST_EXPECT_EQUAL("%d", fixture.fs.num_open, 0);
	mem_reset(&fixture.fs);
}

static btest_suite_t wal = {
	.name = "bwal",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

// A fresh model on an existing log: what a restart does
static bwal_open_result_t
reopen(void) {
	bwal_close(&fixture.wal);
	model_t* model = &fixture.model;
	*model = (model_t){ .refuse_at = -1, .expect_snapshot = true };
	return bwal_open(&fixture.wal, &(bwal_config_t){
		.io = &fixture.io,
		.paths = { "0", "1" },
		.replay_ctx = model,
		.prepare = model_prepare,
		.commit = model_commit,
		.rollback = model_rollback,
	});
}

// A fresh log, compacted, with the values 1 to `count` appended
static void
start_with(int count) {
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_FRESH);
	BTEST_ASSERT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	for (int i = 1; i <= count; ++i) {
		BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, i), (int)BWAL_OK);
	}
}

static const int ONE_TO_FOUR[] = { 1, 2, 3, 4 };

// }}}

BTEST(wal, crc32_is_the_zlib_one) {
	BTEST_EXPECT_EQUAL("%08x", bwal_crc32(0, "123456789", 9), 0xCBF43926u);
	// Continued in pieces, the same
	uint32_t crc = bwal_crc32(0, "1234", 4);
	crc = bwal_crc32(crc, "56789", 5);
	BTEST_EXPECT_EQUAL("%08x", crc, 0xCBF43926u);
}

BTEST(wal, a_fresh_log_takes_nothing_but_a_compaction) {
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_FRESH);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, -1);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_prepares, 0);
	BTEST_EXPECT(!fixture.fs.files[0].exists && !fixture.fs.files[1].exists);

	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);

	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	// Only the order of generations means anything, so the first is simply the one after none
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, 1);
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, HEADER_LEN + ENTRY_OVERHEAD + PAYLOAD_LEN(0));
	BTEST_EXPECT(!fixture.fs.files[1].exists);
}

BTEST(wal, a_log_comes_back_as_it_was) {
	start_with(4);
	size_t len = HEADER_LEN + ENTRY_OVERHEAD + PAYLOAD_LEN(0) + 4 * (ENTRY_OVERHEAD + PAYLOAD_LEN(1));
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, len);

	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 4));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_prepares, 5);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_commits, 5);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 0);
	// Clean, so it goes on where it was
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 5), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, len + ENTRY_OVERHEAD + PAYLOAD_LEN(1));

	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	int expected[] = { 1, 2, 3, 4, 5 };
	BTEST_EXPECT(model_equals(&fixture.model, expected, 5));
}

BTEST(wal, a_torn_tail_is_dropped_and_never_buried) {
	start_with(4);
	mem_fs_t pristine = { 0 };
	mem_copy(&pristine, &fixture.fs);
	size_t len = pristine.files[0].len;
	size_t last_entry = ENTRY_OVERHEAD + PAYLOAD_LEN(1);

	// Cut anywhere inside the last entry: in its magic, its payload or its trailer
	for (size_t cut = len - last_entry + 1; cut < len; ++cut) {
		bwal_close(&fixture.wal);
		mem_copy(&fixture.fs, &pristine);
		fixture.fs.files[0].len = cut;

		BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_TORN);
		BTEST_EXPECT_EX(model_equals(&fixture.model, ONE_TO_FOUR, 3), "cut at %zu", cut);
		BTEST_EXPECT_EX(fixture.wal.state == BWAL_RECOVERED, "cut at %zu", cut);
		BTEST_EXPECT_EX(fixture.model.num_commits == 4, "cut at %zu", cut);
		// Prepare ran on the torn entry only when there was a whole magic to find it by
		int torn_prepares = cut >= len - last_entry + 4 ? 1 : 0;
		BTEST_EXPECT_EX(fixture.model.num_rollbacks == torn_prepares, "cut at %zu", cut);
		BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);

		// The compaction goes to the other slot and leaves the torn one whole
		BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
		BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
		BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
		BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, 2);
		BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, cut);
		BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 4), (int)BWAL_OK);

		BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
		BTEST_EXPECT_EX(model_equals(&fixture.model, ONE_TO_FOUR, 4), "cut at %zu", cut);
		BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
		BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
	}
	mem_reset(&pristine);
}

BTEST(wal, a_bad_entry_ends_the_log_there) {
	start_with(4);
	// Flip a payload byte of the second entry: the first value's low byte
	size_t second = HEADER_LEN + ENTRY_OVERHEAD + PAYLOAD_LEN(0) + ENTRY_OVERHEAD + PAYLOAD_LEN(1);
	fixture.fs.files[0].data[second + 4 + 2] ^= 0xFF;

	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_TORN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 1));
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 1);
	// The entries behind it are not reached
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_prepares, 3);
}

BTEST(wal, a_crashed_compaction_is_rejected_for_the_older_slot) {
	start_with(4);

	// The snapshot gets a header and part of its payload, then the process is gone
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_OK);
	uint8_t head[2] = { 0, 4 };
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_write(&fixture.wal, head, sizeof(head)), (int)BWAL_OK);
	uint8_t one[4];
	put_i32(one, 1);
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_write(&fixture.wal, one, sizeof(one)), (int)BWAL_OK);
	BTEST_EXPECT(fixture.fs.files[1].exists);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.fs.files[1].data[5], 2);  // its generation

	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 4));
	// The newer slot was tried first, its snapshot rolled back, then the older one replayed
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 1);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_commits, 5);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, 1);

	// Appends go on in the older slot
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 5), (int)BWAL_OK);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	int expected[] = { 1, 2, 3, 4, 5 };
	BTEST_EXPECT(model_equals(&fixture.model, expected, 5));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 1);

	// A compaction that completes takes over, and the older slot is kept whole
	size_t older_len = fixture.fs.files[0].len;
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, 2);
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[1].len, HEADER_LEN + ENTRY_OVERHEAD + PAYLOAD_LEN(5));
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, older_len);

	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, expected, 5));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 0);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_commits, 1);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
}

BTEST(wal, generations_wrap) {
	start_with(1);
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 2), (int)BWAL_OK);
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	// Slot 0 logs { 1, 2 } as entries at generation 0, slot 1 as a snapshot at generation 1,
	// so only the slot chosen tells them apart. Age both.
	struct { uint8_t older, newer; } pairs[] = { { 127, 128 }, { 255, 0 }, { 200, 201 } };
	for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
		fixture.fs.files[0].data[5] = pairs[i].older;
		fixture.fs.files[1].data[5] = pairs[i].newer;
		BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
		BTEST_EXPECT_EX(fixture.wal.active_slot == 1, "pair %zu", i);
		BTEST_EXPECT_EX(fixture.wal.generation == pairs[i].newer, "pair %zu", i);
		BTEST_EXPECT_EX(fixture.model.num_values == 2, "pair %zu", i);

		// The next compaction follows the newer one, into the older slot
		BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
		BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
		BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, (int)(uint8_t)(pairs[i].newer + 1));
		BTEST_EXPECT_EQUAL("%d", (int)fixture.fs.files[0].data[5], (int)(uint8_t)(pairs[i].newer + 1));
	}
}

BTEST(wal, short_transfers_are_completed_by_the_library) {
	fixture.fs.max_io = 1;
	start_with(4);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 4));
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 4));
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
}

BTEST(wal, a_failed_append_leaves_only_a_compaction) {
	start_with(2);
	// Enough for the magic and the head, not for the value
	fixture.fs.write_budget = 5;
	size_t len = fixture.fs.files[0].len;
	BTEST_EXPECT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 3), (int)BWAL_ERR_IO);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_EXPECT(fixture.wal.current == NULL);
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, len + 5);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);

	// The value is in memory, so the compaction carries it
	fixture.fs.write_budget = SIZE_MAX;
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	int expected[] = { 1, 2, 3 };
	BTEST_EXPECT(model_equals(&fixture.model, expected, 3));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 0);
}

BTEST(wal, a_failed_compaction_can_be_retried) {
	start_with(2);
	fixture.fs.write_budget = HEADER_LEN + 4 + 3;
	BTEST_EXPECT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_ERR_IO);
	// Back where it was, appending to the current slot
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT(fixture.wal.next == NULL);
	fixture.fs.write_budget = SIZE_MAX;
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 3), (int)BWAL_OK);

	// What it left behind is rejected, and the retry overwrites it
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	int expected[] = { 1, 2, 3 };
	BTEST_EXPECT(model_equals(&fixture.model, expected, 3));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 1);
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, expected, 3));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 0);

	// A compaction from a torn log that fails stays torn
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 4), (int)BWAL_OK);
	fixture.fs.files[1].len -= 1;
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_TORN);
	BTEST_EXPECT(model_equals(&fixture.model, expected, 3));
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	fixture.fs.write_budget = 3;
	BTEST_EXPECT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_ERR_IO);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	fixture.fs.write_budget = SIZE_MAX;
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
}

BTEST(wal, a_refused_entry_ends_the_log_there) {
	start_with(4);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);  // a clean baseline
	bwal_close(&fixture.wal);

	// The third prepare is the second entry
	model_t* model = &fixture.model;
	*model = (model_t){ .refuse_at = 2, .expect_snapshot = true };
	BTEST_ASSERT_EQUAL("%d", (int)bwal_open(&fixture.wal, &(bwal_config_t){
		.io = &fixture.io, .paths = { "0", "1" }, .replay_ctx = model,
		.prepare = model_prepare, .commit = model_commit, .rollback = model_rollback,
	}), (int)BWAL_OPEN_TORN);
	BTEST_EXPECT(model_equals(model, ONE_TO_FOUR, 1));
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_EXPECT_EQUAL("%d", model->num_rollbacks, 1);
	BTEST_EXPECT_EQUAL("%d", model->num_prepares, 3);

	// Refusing the snapshot rejects the slot: with no other, the log is fresh
	bwal_close(&fixture.wal);
	*model = (model_t){ .refuse_at = 0, .expect_snapshot = true };
	BTEST_ASSERT_EQUAL("%d", (int)bwal_open(&fixture.wal, &(bwal_config_t){
		.io = &fixture.io, .paths = { "0", "1" }, .replay_ctx = model,
		.prepare = model_prepare, .commit = model_commit, .rollback = model_rollback,
	}), (int)BWAL_OPEN_FRESH);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, -1);
	BTEST_EXPECT_EQUAL("%d", model->num_values, 0);
	// The next generation still follows the one seen
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, 2);
}

BTEST(wal, reading_past_the_payload_fails_the_entry) {
	start_with(2);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	bwal_close(&fixture.wal);

	model_t* model = &fixture.model;
	*model = (model_t){ .refuse_at = -1, .overread = true, .expect_snapshot = true };
	// The snapshot overread, so the slot is rejected
	BTEST_ASSERT_EQUAL("%d", (int)bwal_open(&fixture.wal, &(bwal_config_t){
		.io = &fixture.io, .paths = { "0", "1" }, .replay_ctx = model,
		.prepare = model_prepare, .commit = model_commit, .rollback = model_rollback,
	}), (int)BWAL_OPEN_FRESH);
	BTEST_EXPECT_EQUAL("%d", model->num_commits, 0);
	BTEST_EXPECT_EQUAL("%d", model->num_rollbacks, 1);
}

BTEST(wal, a_slot_that_will_not_append_is_recovered_not_torn) {
	start_with(2);
	fixture.fs.refuse_append = true;
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_READ_ONLY);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 2));
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT(fixture.wal.current == NULL);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);

	// Truncating still works, so the compaction does
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 3), (int)BWAL_OK);
	fixture.fs.refuse_append = false;
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	int expected[] = { 1, 2, 3 };
	BTEST_EXPECT(model_equals(&fixture.model, expected, 3));
}

BTEST(wal, a_read_error_is_not_a_fresh_log) {
	start_with(2);
	size_t pristine_len = fixture.fs.files[0].len;

	// On a header
	fixture.fs.read_fail_after = 0;
	BTEST_EXPECT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_IO_ERROR);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_CLOSED);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_prepares, 0);

	// Inside an entry: whatever was committed stays, nothing after it is applied
	fixture.fs.read_fail_after = 5;
	BTEST_EXPECT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_IO_ERROR);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_CLOSED);
	BTEST_EXPECT(fixture.model.num_commits < 3);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, fixture.model.num_prepares - fixture.model.num_commits);
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, pristine_len);

	fixture.fs.read_fail_after = SIZE_MAX;
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 2));
}

BTEST(wal, slots_that_are_not_logs_are_ignored) {
	// Garbage in both
	for (int i = 0; i < 2; ++i) {
		mem_file_t* file = &fixture.fs.files[i];
		file->exists = true;
		file->len = file->cap = 16;
		file->data = malloc(16);
		memset(file->data, 0xAB, 16);
	}
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_FRESH);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_prepares, 0);
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.generation, 1);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_commits, 1);

	// A header with nothing after it is a crashed compaction
	bwal_close(&fixture.wal);
	fixture.fs.files[1].exists = true;
	fixture.fs.files[1].len = HEADER_LEN;
	memcpy(fixture.fs.files[1].data, "BWAL\x01\x02", HEADER_LEN);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 0);  // nothing to prepare there

	// An unknown version is not a log either
	bwal_close(&fixture.wal);
	fixture.fs.files[0].data[4] = 2;
	fixture.fs.files[1].len = 0;
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_FRESH);
}

BTEST(wal, an_abandoned_entry_leaves_only_a_compaction) {
	start_with(2);
	size_t len = fixture.fs.files[0].len;
	BTEST_ASSERT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_OK);
	uint8_t byte = 7;
	BTEST_ASSERT_EQUAL("%d", (int)bwal_entry_write(&fixture.wal, &byte, 1), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_rollback(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_EXPECT(fixture.wal.current == NULL);
	BTEST_EXPECT_EQUAL("%zu", fixture.fs.files[0].len, len + 4 + 1);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_rollback(&fixture.wal), (int)BWAL_ERR_INVALID);

	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 1);
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	BTEST_EXPECT(model_equals(&fixture.model, ONE_TO_FOUR, 2));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 0);
}

BTEST(wal, an_abandoned_compaction_goes_back) {
	start_with(2);
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_OK);
	uint8_t byte = 7;
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_write(&fixture.wal, &byte, 1), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_abort(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
	BTEST_EXPECT_EQUAL("%d", fixture.wal.active_slot, 0);
	BTEST_EXPECT(fixture.wal.next == NULL);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_abort(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_ASSERT_EQUAL("%d", (int)model_append(&fixture.model, &fixture.wal, 3), (int)BWAL_OK);

	// What it left is rejected
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_CLEAN);
	int expected[] = { 1, 2, 3 };
	BTEST_EXPECT(model_equals(&fixture.model, expected, 3));
	BTEST_EXPECT_EQUAL("%d", fixture.model.num_rollbacks, 1);

	// From a torn log, back to torn
	fixture.fs.files[0].len -= 1;
	BTEST_ASSERT_EQUAL("%d", (int)reopen(), (int)BWAL_OPEN_TORN);
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_abort(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_RECOVERED);
	BTEST_ASSERT_EQUAL("%d", (int)model_compact(&fixture.model, &fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
}

BTEST(wal, calls_out_of_sequence_are_refused) {
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	uint8_t byte = 0;
	BTEST_EXPECT_EQUAL("%zu", bwal_replay_read(&fixture.wal, &byte, 1), (size_t)0);

	start_with(1);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_write(&fixture.wal, &byte, 1), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_commit(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_write(&fixture.wal, &byte, 1), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_end(&fixture.wal), (int)BWAL_ERR_INVALID);

	BTEST_ASSERT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_APPENDING);
	BTEST_ASSERT_EQUAL("%d", (int)bwal_entry_commit(&fixture.wal), (int)BWAL_OK);

	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_entry_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_EXPECT_EQUAL("%d", (int)bwal_compact_begin(&fixture.wal), (int)BWAL_ERR_INVALID);
	BTEST_ASSERT_EQUAL("%d", (int)bwal_compact_end(&fixture.wal), (int)BWAL_OK);
	BTEST_EXPECT_EQUAL("%d", (int)fixture.wal.state, (int)BWAL_IDLE);
}

#define BLIB_IMPLEMENTATION
#include "../../bwal.h"
