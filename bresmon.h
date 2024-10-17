#ifndef BRESMON_H
#define BRESMON_H

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#	define _DEFAULT_SOURCE 1
#endif

#include <stddef.h>
#include <stdbool.h>

#ifndef BRESMON_API
#define BRESMON_API
#endif

typedef struct bresmon_s bresmon_t;
typedef struct bresmon_watch_s bresmon_watch_t;
typedef struct bresmon_itr_s bresmon_itr_t;
typedef void (*bresmon_callback_t)(const char* file, void* userdata);

#ifdef __cplusplus
extern "C" {
#endif

BRESMON_API bresmon_t*
bresmon_create(void* memctx);

BRESMON_API void
bresmon_destroy(bresmon_t* mon);

BRESMON_API bresmon_watch_t*
bresmon_watch(
	bresmon_t* mon,
	const char* file,
	bresmon_callback_t callback,
	void* userdata
);

BRESMON_API void
bresmon_unwatch(bresmon_t* mon, bresmon_watch_t* watch);

BRESMON_API int
bresmon_check(bresmon_t* mon, bool wait);

BRESMON_API int
bresmon_should_reload(bresmon_t* mon, bool wait);

BRESMON_API int
bresmon_reload(bresmon_t* mon);

#ifdef __cplusplus
}
#endif

#endif

#ifdef BRESMON_IMPLEMENTATION

#if defined(__linux__)

#include <sys/inotify.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <poll.h>

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#else
#error Unsupported platform
#endif

typedef struct bresmon_dirmon_link_s {
	struct bresmon_dirmon_link_s* next;
	struct bresmon_dirmon_link_s* prev;
} bresmon_dirmon_link_t;

typedef struct bresmon_watch_link_s {
	struct bresmon_watch_link_s* next;
	struct bresmon_watch_link_s* prev;
} bresmon_watch_link_t;

typedef struct bresmon_dirmon_s {
	bresmon_dirmon_link_t link;
	bresmon_watch_link_t watches;

#if defined(__linux__)
	int watchd;
#elif defined(_WIN32)
	HANDLE dir_handle;
	OVERLAPPED overlapped;
	_Alignas(FILE_NOTIFY_INFORMATION) char notification_buf[sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH];
#endif
	char path[];
} bresmon_dirmon_t;

struct bresmon_s {
	bresmon_dirmon_link_t dirmons;

	void* memctx;

#if defined(__linux__)
	int inotifyfd;
#elif defined(_WIN32)
	HANDLE iocp;
#endif
};

struct bresmon_watch_s {
	bresmon_watch_link_t link;

	int current_version;
	int latest_version;

	bresmon_dirmon_t* dirmon;

	char* orignal_path;

	bresmon_callback_t callback;
	void* userdata;

#if defined(__linux__)
	char filename[];
#elif defined(_WIN32)
	DWORD filename_len;
	wchar_t filename[];
#endif
};

#ifndef BRESMON_REALLOC
#define BRESMON_REALLOC(ptr, size, ctx) bresmon_libc_realloc(ptr, size, ctx)

#include <stdlib.h>

static inline void*
bresmon_libc_realloc(void* ptr, size_t size, void* ctx) {
	(void)ctx;
	if (size > 0) {
		return realloc(ptr, size);
	} else {
		free(ptr);
		return NULL;
	}
}

#endif

static inline void*
bresmon_malloc(size_t size, void* ctx) {
	return BRESMON_REALLOC(NULL, size, ctx);
}

static inline void
bresmon_free(void* ptr, void* ctx) {
	BRESMON_REALLOC(ptr, 0, ctx);
}

static inline char*
bresmon_strdup(const char* str, void* ctx) {
	size_t size = strlen(str) + 1;
	char* dup = bresmon_malloc(size, ctx);
	memcpy(dup, str, size);
	return dup;
}

bresmon_t*
bresmon_create(void* memctx) {
	bresmon_t* mon = bresmon_malloc(sizeof(bresmon_t), memctx);
	*mon = (bresmon_t){
		.dirmons = {
			.next = &mon->dirmons,
			.prev = &mon->dirmons,
		},
		.memctx = memctx,
#if defined(__linux__)
		.inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC),
#elif defined(_WIN32)
		.iocp = CreateIoCompletionPort(
			INVALID_HANDLE_VALUE,
			NULL,
			0,
			1
		),
#endif
	};

	return mon;
}

void
bresmon_destroy(bresmon_t* mon) {
	while (mon->dirmons.next != &mon->dirmons) {
		bresmon_dirmon_t* dirmon = (bresmon_dirmon_t*)((char*)mon->dirmons.next - offsetof(bresmon_dirmon_t, link));
		if (dirmon->watches.next != &dirmon->watches) {
			bresmon_watch_t* watch = (bresmon_watch_t*)((char*)dirmon->watches.next - offsetof(bresmon_watch_t, link));
			bresmon_unwatch(mon, watch);
		}
	}

#if defined(__linux__)
	close(mon->inotifyfd);
#elif defined(_WIN32)
	CloseHandle(mon->iocp);
#endif

	bresmon_free(mon, mon->memctx);
}
#include <stdio.h>
bresmon_watch_t*
bresmon_watch(
	bresmon_t* mon,
	const char* original_path,
	bresmon_callback_t callback,
	void* userdata
) {
	size_t orignal_path_len = strlen(original_path);

#if defined(__linux__)
	// This will always allocate with libc
	char* real_path = realpath(original_path, NULL);
	char* real_path2 = bresmon_strdup(real_path, mon->memctx);

	char* dir_name = dirname(real_path);
	size_t dir_name_len = strlen(dir_name);

	char* filename = basename(real_path2);
	size_t filename_len = strlen(filename);

	bresmon_dirmon_t* dirmon = NULL;
	for (
		bresmon_dirmon_link_t* itr = mon->dirmons.next;
		itr != &mon->dirmons;
		itr = itr->next
	) {
		bresmon_dirmon_t* dirmon_itr = (bresmon_dirmon_t*)((char*)itr - offsetof(bresmon_dirmon_t, link));
		if (strcmp(dir_name, dirmon_itr->path) == 0) {
			dirmon = dirmon_itr;
			break;
		}
	}

	if (dirmon == NULL) {
		int watchd = inotify_add_watch(
			mon->inotifyfd,
			dir_name,
			IN_CLOSE_WRITE | IN_MOVED_TO
		);

		dirmon = malloc(sizeof(bresmon_dirmon_t) + dir_name_len + 1);
		*dirmon = (bresmon_dirmon_t){
			.watchd = watchd,
			.watches = {
				.next = &dirmon->watches,
				.prev = &dirmon->watches,
			},
		};
		memcpy(dirmon->path, dir_name, dir_name_len + 1);

		dirmon->link.next = mon->dirmons.next;
		mon->dirmons.next->prev = &dirmon->link;
		dirmon->link.prev = &mon->dirmons;
		mon->dirmons.next = &dirmon->link;
	}

	bresmon_watch_t* watch = bresmon_malloc(
		sizeof(bresmon_watch_t)
		+ orignal_path_len + 1
		+ filename_len + 1,
		mon->memctx
	);
	*watch = (bresmon_watch_t){ 0 };
	memcpy(watch->filename, filename, filename_len + 1);
	watch->orignal_path = watch->filename + filename_len + 1;
	memcpy(watch->orignal_path, original_path, orignal_path_len + 1);

	bresmon_free(real_path2, mon->memctx);
	free(real_path);
#elif defined(_WIN32)
	size_t path_buf_size = GetFullPathNameA(original_path, 0, NULL, NULL);
	char* filename;
	char* full_path = bresmon_malloc(path_buf_size, mon->memctx);
	GetFullPathNameA(original_path, (int)path_buf_size, full_path, &filename);
	size_t dir_name_len = filename - full_path;

	size_t filename_len = path_buf_size - 1 - dir_name_len;
	size_t wfilename_buf_len = MultiByteToWideChar(CP_UTF8, 0, filename, (int)(filename_len + 1), NULL, 0);
	bresmon_watch_t* watch = bresmon_malloc(
		sizeof(bresmon_watch_t)
		+ orignal_path_len + 1
		+ wfilename_buf_len * sizeof(wchar_t),
		mon->memctx
	);
	*watch = (bresmon_watch_t){ 0 };
	MultiByteToWideChar(CP_UTF8, 0, filename, (int)(filename_len + 1), watch->filename, (int)wfilename_buf_len);
	watch->filename_len = (int)(wfilename_buf_len - 1);
	watch->orignal_path = (char*)watch->filename + wfilename_buf_len * sizeof(wchar_t);
	memcpy(watch->orignal_path, original_path, orignal_path_len + 1);

	const char* dir_name = full_path;
	*filename = '\0';
	bresmon_dirmon_t* dirmon = NULL;
	for (
		bresmon_dirmon_link_t* itr = mon->dirmons.next;
		itr != &mon->dirmons;
		itr = itr->next
	) {
		bresmon_dirmon_t* dirmon_itr = (bresmon_dirmon_t*)((char*)itr - offsetof(bresmon_dirmon_t, link));
		if (_stricmp(dir_name, dirmon_itr->path) == 0) {
			dirmon = dirmon_itr;
			break;
		}
	}

	if (dirmon == NULL) {
		dirmon = malloc(sizeof(bresmon_dirmon_t) + dir_name_len + 1);
		*dirmon = (bresmon_dirmon_t){
			.watches = {
				.next = &dirmon->watches,
				.prev = &dirmon->watches,
			},
		};
		memcpy(dirmon->path, dir_name, dir_name_len + 1);

		dirmon->link.next = mon->dirmons.next;
		mon->dirmons.next->prev = &dirmon->link;
		dirmon->link.prev = &mon->dirmons;
		mon->dirmons.next = &dirmon->link;

		dirmon->dir_handle = CreateFileA(
			dir_name,
			FILE_LIST_DIRECTORY,
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL
		);
		CreateIoCompletionPort(dirmon->dir_handle, mon->iocp, 0, 1);
		ReadDirectoryChangesW(
			dirmon->dir_handle,
			dirmon->notification_buf,
			sizeof(dirmon->notification_buf),
			FALSE,
			FILE_NOTIFY_CHANGE_FILE_NAME
			| FILE_NOTIFY_CHANGE_LAST_WRITE,
			NULL,
			&dirmon->overlapped,
			NULL
		);
	}

	bresmon_free(full_path, mon->memctx);
#endif

	watch->link.next = &dirmon->watches;
	watch->link.prev = dirmon->watches.prev;
	dirmon->watches.prev->next = &watch->link;
	dirmon->watches.prev = &watch->link;

	watch->dirmon = dirmon;
	watch->callback = callback;
	watch->userdata = userdata;

	return watch;
}

void
bresmon_unwatch(bresmon_t* mon, bresmon_watch_t* watch) {
	watch->link.prev->next = watch->link.next;
	watch->link.next->prev = watch->link.prev;

	bresmon_dirmon_t* dirmon = watch->dirmon;
	if (dirmon->watches.next == &dirmon->watches) {
		dirmon->link.next->prev = dirmon->link.prev;
		dirmon->link.prev->next = dirmon->link.next;

#if defined(__linux__)
		inotify_rm_watch(mon->inotifyfd, dirmon->watchd);
#elif defined(_WIN32)
		CancelIo(dirmon->dir_handle);
		CloseHandle(dirmon->dir_handle);
#endif

		bresmon_free(dirmon, mon->memctx);
	}

	bresmon_free(watch, mon->memctx);
}

int
bresmon_check(bresmon_t* mon, bool wait) {
	if (bresmon_should_reload(mon, wait) > 0) {
		return bresmon_reload(mon);
	} else {
		return 0;
	}
}

int
bresmon_should_reload(bresmon_t* mon, bool wait) {
	int num_events = 0;

#if defined(__linux__)
	if (wait) {
		struct pollfd pollfd = {
			.fd = mon->inotifyfd,
			.events = POLLIN,
		};
		poll(&pollfd, 1, -1);
	}

	_Alignas(struct inotify_event) char event_buf[sizeof(struct inotify_event) + NAME_MAX + 1];

	while (true) {
		ssize_t num_bytes_read = read(mon->inotifyfd, event_buf, sizeof(event_buf));

		if (num_bytes_read <= 0) {
			break;
		}

		for (
			char* event_itr = event_buf;
			event_itr - event_buf < num_bytes_read;
		) {
			struct inotify_event* event = (struct inotify_event*)event_itr;
			event_itr += sizeof(struct inotify_event) + event->len;

			for (
				bresmon_dirmon_link_t* itr = mon->dirmons.next;
				itr != &mon->dirmons;
				itr = itr->next
			) {
				bresmon_dirmon_t* dirmon = (bresmon_dirmon_t*)((char*)itr - offsetof(bresmon_dirmon_t, link));

				if (dirmon->watchd == event->wd) {
					for (
						bresmon_watch_link_t* watch_itr = dirmon->watches.next;
						watch_itr != &dirmon->watches;
						watch_itr = watch_itr->next
					) {
						bresmon_watch_t* watch = (bresmon_watch_t*)((char*)watch_itr - offsetof(bresmon_watch_t, link));
						if (strcmp(watch->filename, event->name) == 0) {
							++watch->latest_version;
							++num_events;
						}
					}

					break;
				}
			}
		}
	}
#elif defined(_WIN32)
	DWORD num_bytes;
	ULONG_PTR key;
	OVERLAPPED* overlapped;

	while (GetQueuedCompletionStatus(mon->iocp, &num_bytes, &key, &overlapped, wait ? INFINITE : 0)) {
		wait = false;  // Only wait for the initial event
		for (
			bresmon_dirmon_link_t* itr = mon->dirmons.next;
			itr != &mon->dirmons;
			itr = itr->next
		) {
			bresmon_dirmon_t* dirmon = (bresmon_dirmon_t*)((char*)itr - offsetof(bresmon_dirmon_t, link));

			if (&dirmon->overlapped != overlapped) { continue; }

			for (
				FILE_NOTIFY_INFORMATION* notification_itr = (FILE_NOTIFY_INFORMATION*)dirmon->notification_buf;
				notification_itr != NULL;
				notification_itr = notification_itr->NextEntryOffset != 0
					? (FILE_NOTIFY_INFORMATION*)((char*)notification_itr + notification_itr->NextEntryOffset)
					: NULL
			) {
				if (notification_itr->Action == FILE_ACTION_RENAMED_OLD_NAME) { continue; }

				for (
					bresmon_watch_link_t* watch_itr = dirmon->watches.next;
					watch_itr != &dirmon->watches;
					watch_itr = watch_itr->next
				) {
					bresmon_watch_t* watch = (bresmon_watch_t*)((char*)watch_itr - offsetof(bresmon_watch_t, link));
					if (
						watch->filename_len == (notification_itr->FileNameLength / sizeof(wchar_t))
						&& wcsncmp(watch->filename, notification_itr->FileName, watch->filename_len) == 0
					) {
						++watch->latest_version;
						++num_events;
					}
				}
			}

			// Queue another read
			ReadDirectoryChangesW(
				dirmon->dir_handle,
				dirmon->notification_buf,
				sizeof(dirmon->notification_buf),
				FALSE,
				FILE_NOTIFY_CHANGE_FILE_NAME
				| FILE_NOTIFY_CHANGE_LAST_WRITE,
				NULL,
				&dirmon->overlapped,
				NULL
			);
			break;
		}
	}
#endif

	return num_events;
}

int
bresmon_reload(bresmon_t* mon) {
	int num_reloads = 0;
	for (
		bresmon_dirmon_link_t* itr = mon->dirmons.next;
		itr != &mon->dirmons;
		itr = itr->next
	) {
		bresmon_dirmon_t* dirmon = (bresmon_dirmon_t*)((char*)itr - offsetof(bresmon_dirmon_t, link));
		for (
			bresmon_watch_link_t* watch_itr = dirmon->watches.next;
			watch_itr != &dirmon->watches;
			watch_itr = watch_itr->next
		) {
			bresmon_watch_t* watch = (bresmon_watch_t*)((char*)watch_itr - offsetof(bresmon_watch_t, link));

			if (watch->current_version != watch->latest_version) {
				++num_reloads;
				watch->current_version = watch->latest_version;
				watch->callback(watch->orignal_path, watch->userdata);
			}
		}
	}

	return num_reloads;
}

#endif