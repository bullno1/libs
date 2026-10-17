#ifndef BWAL_H
#define BWAL_H

/**
 * @file
 * Write-ahead log over two alternating files.
 *
 * Designed for file systems with no rename, no truncate and no read-write handle,
 * such as PhysicsFS.
 *
 * ## On disk format
 *
 * The library maintains two log files.
 * They alternate as the active slot.
 * Each contains a header, a snapshot entry, then incremental entries:
 *
 * ```
 * [magic "BWAL"][version][generation]
 * [magic "BWEN"][payload][payload_len][crc32]   // snapshot
 * [magic "BWEN"][payload][payload_len][crc32]   // entry
 * [magic "BWEN"][payload][payload_len][crc32]   // another entry
 * ...
 * ```
 *
 * The length is in the trailer, so the caller can stream a payload straight
 * into the log without buffering.
 * The crc32 covers the entry magic and the payload.
 *
 * The payload must be self delimiting.
 * During replay, the caller must read the whole payload without looking ahead.
 * Then, the library will take over and verifies the checksum and size at the
 * trailer.
 *
 * ## Recovery
 *
 * @ref bwal_open reads both logs and tries the one with a higher generation first.
 * A log whose snapshot does not verify is rejected and the other log is
 * replayed instead.
 *
 * Any later entry that does not verify terminates replay.
 *
 * A log that ended cleanly is continued.
 * A corrupted tail cannot be truncated so the caller must immediately compact
 * the log after opening.
 *
 * ### Compaction
 *
 * The caller streams a snapshot into the other log slot with the next generation.
 * The old log stays intact until the next compaction.
 * Therefore, at most two generations of logs are always on disk.
 *
 * ### Durability
 *
 * Every commit ends with a backend flush.
 * Durability guarantees are up to the backend: whether it does the equivalence
 * of fsync or just fflush.
 *
 * Usage:
 *
 * @code{.c}
 * bwal_t wal;
 * bwal_open_result_t opened = bwal_open(&wal, &(bwal_config_t){
 *     .io = &my_io,
 *     .paths = { "save.0", "save.1" },
 *     .replay_ctx = &state,
 *     .prepare = prepare_entry,
 *     .commit = apply_entry,
 *     .rollback = drop_entry,
 * });
 * if (opened == BWAL_OPEN_FRESH) { init_state(&state); }
 * if (wal.state == BWAL_RECOVERED) {
 *     bwal_compact_begin(&wal);
 *     write_snapshot(&state, &wal);  // using bwal_compact_write
 *     bwal_compact_end(&wal);
 * }
 * bwal_entry_begin(&wal);
 * bwal_entry_write(&wal, bytes, len);
 * bwal_entry_commit(&wal);
 * @endcode
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef BWAL_API
#define BWAL_API
#endif

typedef struct bwal_s bwal_t;

typedef enum {
	BWAL_OK = 0,
	/// The backend failed
	BWAL_ERR_IO,
	/// A call out of sequence, or from prepare: an entry the caller refuses
	BWAL_ERR_INVALID,
} bwal_err_t;

// I/O {{{

/// A file handle, opaque to the library
typedef struct bwal_file_s bwal_file_t;

typedef enum {
	BWAL_OPEN_READ,
	/// Create or continue at the end
	BWAL_OPEN_APPEND,
	/// Create or empty, then write from the start
	BWAL_OPEN_TRUNCATE,
} bwal_open_mode_t;

/**
 * The file system.
 *
 * Must stay valid for the lifetime of the @ref bwal_t.
 */
typedef struct {
	void* userdata;

	/// NULL when the file cannot be opened
	bwal_file_t* (*open)(void* userdata, const char* path, bwal_open_mode_t mode);

	/**
	 * Bytes read, 0 at the end of the file, negative on an error.
	 *
	 * An error must never be reported as the end of the file.
	 */
	int64_t (*read)(bwal_file_t* file, void* buf, size_t len);

	/// Bytes written, 0 on an error. Fewer than asked is not an error.
	size_t (*write)(bwal_file_t* file, const void* buf, size_t len);

	/// Optional. False on an error.
	bool (*flush)(bwal_file_t* file);

	void (*close)(bwal_file_t* file);
} bwal_io_t;

// }}}

// Replay {{{

/**
 * Called once per entry during @ref bwal_open, before the entry is verified.
 *
 * Reads the payload through @ref bwal_replay_read and stages it. Must not
 * touch the real state, since the entry may still be rolled back.
 *
 * Return @ref BWAL_ERR_INVALID to refuse the entry.
 */
typedef bwal_err_t (*bwal_prepare_fn_t)(void* ctx, bwal_t* wal);

/// The entry verified: apply what prepare staged. Cannot fail.
typedef void (*bwal_commit_fn_t)(void* ctx);

/**
 * The entry did not verify or prepare refused it
 *
 * May run more than once per open.
 */
typedef void (*bwal_rollback_fn_t)(void* ctx);

// }}}

// Configuration {{{

typedef struct {
	/// Borrowed for the life of the @ref bwal_t
	const bwal_io_t* io;
	/// Borrowed for the life of the @ref bwal_t
	const char* paths[2];

	/// Used during @ref bwal_open only
	void* replay_ctx;
	bwal_prepare_fn_t prepare;
	bwal_commit_fn_t commit;
	bwal_rollback_fn_t rollback;
} bwal_config_t;

// }}}

// Handle {{{

typedef enum {
	BWAL_CLOSED = 0,
	/// Inside prepare
	BWAL_REPLAYING,
	/// Nothing to append to: @ref bwal_compact_begin is the only legal call
	BWAL_RECOVERED,
	/// @ref bwal_entry_begin or @ref bwal_compact_begin can be called
	BWAL_IDLE,
	/// Between @ref bwal_entry_begin and @ref bwal_entry_commit
	BWAL_APPENDING,
	/// Between @ref bwal_compact_begin and @ref bwal_compact_end
	BWAL_COMPACTING,
} bwal_state_t;

/**
 * The log.
 *
 * Exposed for static allocation. Read, never written, by the caller.
 */
struct bwal_s {
	bwal_state_t state;
	/// 0 or 1, -1 while no slot holds the log
	int active_slot;
	/// Of the active slot. Wraps; only the order between the two slots means anything.
	uint8_t generation;

	// Internal
	const bwal_io_t* io;
	const char* paths[2];
	bwal_file_t* current;
	bwal_file_t* next;
	int next_slot;
	uint32_t crc;
	size_t bytes;
	bool read_failed;
};

// }}}

/// What @ref bwal_open found, and the state it left the log in
typedef enum {
	/// No usable log, nothing was committed. @ref BWAL_RECOVERED.
	BWAL_OPEN_FRESH,
	/// The log was replayed whole and can be appended to. @ref BWAL_IDLE.
	BWAL_OPEN_CLEAN,
	/// The log was replayed up to an entry that did not verify. @ref BWAL_RECOVERED.
	BWAL_OPEN_TORN,
	/// The log was replayed whole but cannot be appended to. @ref BWAL_RECOVERED.
	BWAL_OPEN_READ_ONLY,
	/// The backend failed. Nothing was applied after the last commit. @ref BWAL_CLOSED.
	BWAL_OPEN_IO_ERROR,
} bwal_open_result_t;

/**
 * Open the log and replay it through the callbacks.
 *
 * Valid on a @ref bwal_t that is zeroed or @ref BWAL_CLOSED.
 */
BWAL_API bwal_open_result_t
bwal_open(bwal_t* wal, const bwal_config_t* config);

/**
 * Close the log. Nothing on disk changes.
 *
 * Valid in any state but @ref BWAL_REPLAYING. The state becomes @ref BWAL_CLOSED.
 */
BWAL_API void
bwal_close(bwal_t* wal);

/**
 * Read payload bytes of the entry being prepared.
 *
 * Returns the number of bytes read, fewer than asked only at the end of the
 * payload or on an error.
 *
 * Valid inside @ref bwal_prepare_fn_t only.
 */
BWAL_API size_t
bwal_replay_read(bwal_t* wal, void* buf, size_t len);

// Append {{{

/**
 * Begin an entry.
 *
 * Valid in @ref BWAL_IDLE. On success the state becomes @ref BWAL_APPENDING.
 */
BWAL_API bwal_err_t
bwal_entry_begin(bwal_t* wal);

/**
 * Append payload bytes to the entry.
 *
 * Valid in @ref BWAL_APPENDING.
 */
BWAL_API bwal_err_t
bwal_entry_write(bwal_t* wal, const void* buf, size_t len);

/**
 * Commit the entry.
 *
 * The entry exists once this returns @ref BWAL_OK and not before.
 *
 * Valid in @ref BWAL_APPENDING. On success the state becomes @ref BWAL_IDLE.
 * On @ref BWAL_ERR_IO, here or in @ref bwal_entry_write, the state becomes
 * @ref BWAL_RECOVERED and a compaction must follow.
 */
BWAL_API bwal_err_t
bwal_entry_commit(bwal_t* wal);

/**
 * Roll the entry back.
 *
 * Nothing of it is seen again, but it cannot be cut from the file, so the state
 * becomes @ref BWAL_RECOVERED and a compaction must follow.
 *
 * Valid in @ref BWAL_APPENDING.
 */
BWAL_API bwal_err_t
bwal_entry_rollback(bwal_t* wal);

// }}}

// Compaction {{{

/**
 * Begin a compaction: a snapshot that replaces the log.
 *
 * Valid in @ref BWAL_IDLE or @ref BWAL_RECOVERED. On success the state becomes
 * @ref BWAL_COMPACTING.
 */
BWAL_API bwal_err_t
bwal_compact_begin(bwal_t* wal);

/**
 * Append bytes to the snapshot.
 *
 * Valid in @ref BWAL_COMPACTING.
 */
BWAL_API bwal_err_t
bwal_compact_write(bwal_t* wal, const void* buf, size_t len);

/**
 * Commit the compaction.
 *
 * Valid in @ref BWAL_COMPACTING. On success the state becomes @ref BWAL_IDLE.
 * On @ref BWAL_ERR_IO, here or in @ref bwal_compact_write, the state returns
 * to what it was before @ref bwal_compact_begin and the compaction can be
 * retried.
 */
BWAL_API bwal_err_t
bwal_compact_end(bwal_t* wal);

/**
 * Abandon the compaction.
 *
 * Valid in @ref BWAL_COMPACTING. The state returns to what it was before
 * @ref bwal_compact_begin.
 */
BWAL_API bwal_err_t
bwal_compact_abort(bwal_t* wal);

// }}}

/// The checksum of the format. Pass 0 to start and the result to continue.
BWAL_API uint32_t
bwal_crc32(uint32_t crc, const void* buf, size_t len);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BWAL_IMPLEMENTATION)
#define BWAL_IMPLEMENTATION
#endif

#ifdef BWAL_IMPLEMENTATION

#include <string.h>

#define BWAL__FILE_MAGIC "BWAL"
#define BWAL__ENTRY_MAGIC "BWEN"
#define BWAL__MAGIC_LEN 4
#define BWAL__VERSION 1
#define BWAL__HEADER_LEN (BWAL__MAGIC_LEN + 2)
#define BWAL__TRAILER_LEN 8

// Checksum {{{

uint32_t
bwal_crc32(uint32_t crc, const void* buf, size_t len) {
	static uint32_t table[256];
	static bool table_ready = false;
	if (!table_ready) {
		for (uint32_t i = 0; i < 256; ++i) {
			uint32_t c = i;
			for (int k = 0; k < 8; ++k) {
				c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			}
			table[i] = c;
		}
		table_ready = true;
	}

	const uint8_t* bytes = buf;
	crc = ~crc;
	for (size_t i = 0; i < len; ++i) {
		crc = table[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
	}
	return ~crc;
}

// }}}

// I/O loops {{{

typedef enum {
	BWAL__READ_OK,
	BWAL__READ_SHORT,
	BWAL__READ_ERROR,
} bwal__read_result_t;

// Fill the buffer or report why not. `*num_read` is what arrived.
static bwal__read_result_t
bwal__read_fully(bwal_t* wal, bwal_file_t* file, void* buf, size_t len, size_t* num_read) {
	size_t total = 0;
	while (total < len) {
		int64_t n = wal->io->read(file, (uint8_t*)buf + total, len - total);
		if (n < 0) {
			if (num_read != NULL) { *num_read = total; }
			return BWAL__READ_ERROR;
		}
		if (n == 0) { break; }
		total += (size_t)n;
	}
	if (num_read != NULL) { *num_read = total; }
	return total == len ? BWAL__READ_OK : BWAL__READ_SHORT;
}

static bool
bwal__write_fully(bwal_t* wal, bwal_file_t* file, const void* buf, size_t len) {
	size_t total = 0;
	while (total < len) {
		size_t n = wal->io->write(file, (const uint8_t*)buf + total, len - total);
		if (n == 0) { return false; }
		total += n;
	}
	return true;
}

static bool
bwal__flush(bwal_t* wal, bwal_file_t* file) {
	return wal->io->flush == NULL || wal->io->flush(file);
}

static void
bwal__put_u32(uint8_t* out, uint32_t value) {
	out[0] = (uint8_t)(value & 0xFFu);
	out[1] = (uint8_t)((value >> 8) & 0xFFu);
	out[2] = (uint8_t)((value >> 16) & 0xFFu);
	out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint32_t
bwal__get_u32(const uint8_t* in) {
	return (uint32_t)in[0]
		| ((uint32_t)in[1] << 8)
		| ((uint32_t)in[2] << 16)
		| ((uint32_t)in[3] << 24);
}

// }}}

// Entries {{{

// The head of an entry: magic, and the checksum starts over it
static bool
bwal__write_entry_begin(bwal_t* wal, bwal_file_t* file) {
	wal->crc = bwal_crc32(0, BWAL__ENTRY_MAGIC, BWAL__MAGIC_LEN);
	wal->bytes = 0;
	return bwal__write_fully(wal, file, BWAL__ENTRY_MAGIC, BWAL__MAGIC_LEN);
}

static bool
bwal__write_payload(bwal_t* wal, bwal_file_t* file, const void* buf, size_t len) {
	if (!bwal__write_fully(wal, file, buf, len)) { return false; }
	wal->crc = bwal_crc32(wal->crc, buf, len);
	wal->bytes += len;
	return true;
}

// The trailer, then the flush: the entry exists once both succeeded
static bool
bwal__write_entry_commit(bwal_t* wal, bwal_file_t* file) {
	if ((uint64_t)wal->bytes > UINT32_MAX) { return false; }
	uint8_t trailer[BWAL__TRAILER_LEN];
	bwal__put_u32(trailer, (uint32_t)wal->bytes);
	bwal__put_u32(trailer + 4, wal->crc);
	return bwal__write_fully(wal, file, trailer, BWAL__TRAILER_LEN) && bwal__flush(wal, file);
}

// }}}

// Recovery {{{

typedef enum {
	// An entry verified and was committed
	BWAL__COMMITTED,
	// Nothing where an entry would start: the log ended cleanly
	BWAL__CLEAN_END,
	BWAL__TORN,
	// The snapshot did not verify: nothing of this slot is usable
	BWAL__REJECTED,
	BWAL__IO_ERROR,
} bwal__result_t;

typedef enum {
	BWAL__HEADER_OK,
	// Absent, or not a log
	BWAL__HEADER_NONE,
	BWAL__HEADER_IO_ERROR,
} bwal__header_result_t;

// A read error is not an absent file: taken for one, it would make a good log look fresh
static bwal__header_result_t
bwal__read_header(bwal_t* wal, int slot, uint8_t* generation) {
	bwal_file_t* file = wal->io->open(wal->io->userdata, wal->paths[slot], BWAL_OPEN_READ);
	if (file == NULL) { return BWAL__HEADER_NONE; }

	uint8_t header[BWAL__HEADER_LEN];
	bwal__read_result_t read = bwal__read_fully(wal, file, header, sizeof(header), NULL);
	wal->io->close(file);

	if (read == BWAL__READ_ERROR) { return BWAL__HEADER_IO_ERROR; }
	bool ok = read == BWAL__READ_OK
		&& memcmp(header, BWAL__FILE_MAGIC, BWAL__MAGIC_LEN) == 0
		&& header[BWAL__MAGIC_LEN] == BWAL__VERSION;
	if (!ok) { return BWAL__HEADER_NONE; }

	*generation = header[BWAL__MAGIC_LEN + 1];
	return BWAL__HEADER_OK;
}

static bool
bwal__newer(uint8_t a, uint8_t b) {
	return (int8_t)(uint8_t)(a - b) > 0;
}

// Replays one entry
static bwal__result_t
bwal__replay_entry(bwal_t* wal, bwal_file_t* file, const bwal_config_t* config, bool first) {
	bwal__result_t failure = first ? BWAL__REJECTED : BWAL__TORN;

	uint8_t magic[BWAL__MAGIC_LEN];
	size_t num_read;
	switch (bwal__read_fully(wal, file, magic, sizeof(magic), &num_read)) {
		case BWAL__READ_OK:
			break;
		case BWAL__READ_SHORT:
			if (num_read == 0 && !first) { return BWAL__CLEAN_END; }
			return failure;
		case BWAL__READ_ERROR:
			return BWAL__IO_ERROR;
	}
	if (memcmp(magic, BWAL__ENTRY_MAGIC, BWAL__MAGIC_LEN) != 0) { return failure; }

	wal->crc = bwal_crc32(0, magic, sizeof(magic));
	wal->bytes = 0;
	wal->read_failed = false;
	wal->current = file;
	wal->state = BWAL_REPLAYING;
	bwal_err_t err = config->prepare(config->replay_ctx, wal);
	wal->state = BWAL_CLOSED;
	wal->current = NULL;

	if (wal->read_failed) {
		config->rollback(config->replay_ctx);
		return BWAL__IO_ERROR;
	}
	if (err != BWAL_OK) {
		config->rollback(config->replay_ctx);
		return failure;
	}

	uint8_t trailer[BWAL__TRAILER_LEN];
	switch (bwal__read_fully(wal, file, trailer, sizeof(trailer), NULL)) {
		case BWAL__READ_OK:
			break;
		case BWAL__READ_SHORT:
			config->rollback(config->replay_ctx);
			return failure;
		case BWAL__READ_ERROR:
			config->rollback(config->replay_ctx);
			return BWAL__IO_ERROR;
	}

	bool verified = (uint64_t)wal->bytes <= UINT32_MAX
		&& bwal__get_u32(trailer) == (uint32_t)wal->bytes
		&& bwal__get_u32(trailer + 4) == wal->crc;
	if (!verified) {
		config->rollback(config->replay_ctx);
		return failure;
	}

	config->commit(config->replay_ctx);
	return BWAL__COMMITTED;
}

// Replays a slot whose header was read: the snapshot, then every entry to the end.
// Never BWAL__COMMITTED.
static bwal__result_t
bwal__replay_slot(bwal_t* wal, int slot, const bwal_config_t* config) {
	bwal_file_t* file = wal->io->open(wal->io->userdata, wal->paths[slot], BWAL_OPEN_READ);
	if (file == NULL) { return BWAL__IO_ERROR; }

	uint8_t header[BWAL__HEADER_LEN];
	bwal__result_t result;
	if (bwal__read_fully(wal, file, header, sizeof(header), NULL) != BWAL__READ_OK) {
		result = BWAL__IO_ERROR;
	} else {
		result = bwal__replay_entry(wal, file, config, true);
		while (result == BWAL__COMMITTED) {
			result = bwal__replay_entry(wal, file, config, false);
		}
	}

	wal->io->close(file);
	return result;
}

// }}}

// Lifetime {{{

bwal_open_result_t
bwal_open(bwal_t* wal, const bwal_config_t* config) {
	*wal = (bwal_t){
		.io = config->io,
		.paths = { config->paths[0], config->paths[1] },
		.state = BWAL_CLOSED,
		.active_slot = -1,
	};

	// Which slots are logs, and in which order to try them. The highest generation seen
	// is what a compaction follows should neither be usable.
	uint8_t generations[2];
	bool valid[2];
	bool any_valid = false;
	for (int slot = 0; slot < 2; ++slot) {
		switch (bwal__read_header(wal, slot, &generations[slot])) {
			case BWAL__HEADER_OK:
				valid[slot] = true;
				break;
			case BWAL__HEADER_NONE:
				valid[slot] = false;
				break;
			case BWAL__HEADER_IO_ERROR:
				return BWAL_OPEN_IO_ERROR;
		}
		if (valid[slot] && (!any_valid || bwal__newer(generations[slot], wal->generation))) {
			wal->generation = generations[slot];
			any_valid = true;
		}
	}
	int order[2];
	int num_candidates = 0;
	if (valid[0] && valid[1]) {
		int newer = bwal__newer(generations[0], generations[1]) ? 0 : 1;
		order[num_candidates++] = newer;
		order[num_candidates++] = 1 - newer;
	} else if (valid[0] || valid[1]) {
		order[num_candidates++] = valid[0] ? 0 : 1;
	}

	for (int i = 0; i < num_candidates; ++i) {
		int slot = order[i];
		bwal__result_t result = bwal__replay_slot(wal, slot, config);
		if (result == BWAL__IO_ERROR) {
			wal->state = BWAL_CLOSED;
			return BWAL_OPEN_IO_ERROR;
		}
		if (result == BWAL__REJECTED) {
			// The other slot, if any, from its snapshot
			continue;
		}

		wal->active_slot = slot;
		wal->generation = generations[slot];
		if (result == BWAL__TORN) {
			wal->state = BWAL_RECOVERED;
			return BWAL_OPEN_TORN;
		}

		wal->current = wal->io->open(wal->io->userdata, wal->paths[slot], BWAL_OPEN_APPEND);
		if (wal->current == NULL) {
			wal->state = BWAL_RECOVERED;
			return BWAL_OPEN_READ_ONLY;
		}
		wal->state = BWAL_IDLE;
		return BWAL_OPEN_CLEAN;
	}

	wal->state = BWAL_RECOVERED;
	return BWAL_OPEN_FRESH;
}

void
bwal_close(bwal_t* wal) {
	if (wal->current != NULL) { wal->io->close(wal->current); }
	if (wal->next != NULL) { wal->io->close(wal->next); }
	wal->current = NULL;
	wal->next = NULL;
	wal->state = BWAL_CLOSED;
}

size_t
bwal_replay_read(bwal_t* wal, void* buf, size_t len) {
	if (wal->state != BWAL_REPLAYING || wal->read_failed) { return 0; }

	size_t num_read;
	if (bwal__read_fully(wal, wal->current, buf, len, &num_read) == BWAL__READ_ERROR) {
		wal->read_failed = true;
	}
	wal->crc = bwal_crc32(wal->crc, buf, num_read);
	wal->bytes += num_read;
	return num_read;
}

// }}}

// Append {{{

// The slot has a torn entry at its end now: nothing may follow it
static bwal_err_t
bwal__append_failed(bwal_t* wal) {
	wal->io->close(wal->current);
	wal->current = NULL;
	wal->state = BWAL_RECOVERED;
	return BWAL_ERR_IO;
}

bwal_err_t
bwal_entry_begin(bwal_t* wal) {
	if (wal->state != BWAL_IDLE) { return BWAL_ERR_INVALID; }
	if (!bwal__write_entry_begin(wal, wal->current)) { return bwal__append_failed(wal); }
	wal->state = BWAL_APPENDING;
	return BWAL_OK;
}

bwal_err_t
bwal_entry_write(bwal_t* wal, const void* buf, size_t len) {
	if (wal->state != BWAL_APPENDING) { return BWAL_ERR_INVALID; }
	if (!bwal__write_payload(wal, wal->current, buf, len)) { return bwal__append_failed(wal); }
	return BWAL_OK;
}

bwal_err_t
bwal_entry_commit(bwal_t* wal) {
	if (wal->state != BWAL_APPENDING) { return BWAL_ERR_INVALID; }
	if (!bwal__write_entry_commit(wal, wal->current)) { return bwal__append_failed(wal); }
	wal->state = BWAL_IDLE;
	return BWAL_OK;
}

bwal_err_t
bwal_entry_rollback(bwal_t* wal) {
	if (wal->state != BWAL_APPENDING) { return BWAL_ERR_INVALID; }
	bwal__append_failed(wal);
	return BWAL_OK;
}

// }}}

// Compaction {{{

// Back to before compact_begin, with the slot it wrote left for open to reject
static bwal_err_t
bwal__compaction_failed(bwal_t* wal) {
	wal->io->close(wal->next);
	wal->next = NULL;
	wal->state = wal->current != NULL ? BWAL_IDLE : BWAL_RECOVERED;
	return BWAL_ERR_IO;
}

bwal_err_t
bwal_compact_begin(bwal_t* wal) {
	if (wal->state != BWAL_IDLE && wal->state != BWAL_RECOVERED) { return BWAL_ERR_INVALID; }

	int slot = wal->active_slot < 0 ? 0 : 1 - wal->active_slot;
	bwal_file_t* file = wal->io->open(wal->io->userdata, wal->paths[slot], BWAL_OPEN_TRUNCATE);
	if (file == NULL) { return BWAL_ERR_IO; }
	wal->next = file;
	wal->next_slot = slot;
	wal->state = BWAL_COMPACTING;

	uint8_t header[BWAL__HEADER_LEN];
	memcpy(header, BWAL__FILE_MAGIC, BWAL__MAGIC_LEN);
	header[BWAL__MAGIC_LEN] = BWAL__VERSION;
	header[BWAL__MAGIC_LEN + 1] = (uint8_t)(wal->generation + 1);
	if (!bwal__write_fully(wal, file, header, sizeof(header))) { return bwal__compaction_failed(wal); }
	if (!bwal__write_entry_begin(wal, file)) { return bwal__compaction_failed(wal); }

	return BWAL_OK;
}

bwal_err_t
bwal_compact_write(bwal_t* wal, const void* buf, size_t len) {
	if (wal->state != BWAL_COMPACTING) { return BWAL_ERR_INVALID; }
	if (!bwal__write_payload(wal, wal->next, buf, len)) { return bwal__compaction_failed(wal); }
	return BWAL_OK;
}

bwal_err_t
bwal_compact_end(bwal_t* wal) {
	if (wal->state != BWAL_COMPACTING) { return BWAL_ERR_INVALID; }
	if (!bwal__write_entry_commit(wal, wal->next)) { return bwal__compaction_failed(wal); }

	if (wal->current != NULL) { wal->io->close(wal->current); }
	wal->current = wal->next;
	wal->next = NULL;
	wal->active_slot = wal->next_slot;
	wal->generation = (uint8_t)(wal->generation + 1);
	wal->state = BWAL_IDLE;
	return BWAL_OK;
}

bwal_err_t
bwal_compact_abort(bwal_t* wal) {
	if (wal->state != BWAL_COMPACTING) { return BWAL_ERR_INVALID; }
	bwal__compaction_failed(wal);
	return BWAL_OK;
}

// }}}

#endif
