#ifndef BSPSCQ_H
#define BSPSCQ_H

/**
 * @file
 * @brief Lock-free single-producer single-consumer (spsc) queue.
 *
 * No memory allocation is made and the user is responsible for managing the
 * queue's storage as well as messages' lifecycle.
 * Since the queue has a fixed size, it is possible to just preallocate the
 * messages and use them in a round-robin manner from the producer.
 * Ideally, the preallocated buffer should be at least the size of the queue
 * plus 2.
 * This is because in the worst case:
 *
 * * The queue is full.
 * * One message is being prepared by the producer.
 * * One message is being processed by the consumer.
 *
 * The queue can optionally block on production or consumption.
 * This makes it suitable to be used as a message queue to a background thread
 * that waits for jobs to be dispatched instead of busy spinning.
 *
 * Based on: https://github.com/mattiasgustavsson/libs/blob/main/thread.h.
 * Using C11 threading and atomic primitives instead of platform-specific API.
 */

#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>

#ifndef BSPSCQ_API
#define BSPSCQ_API
#endif

typedef struct {
	mtx_t mtx;
	cnd_t cnd;
} bspscq_signal_t;

typedef struct bspscq_s {
	bspscq_signal_t can_produce;
	bspscq_signal_t can_consume;
	atomic_int count;
	atomic_uint head;
	atomic_uint tail;
	void** values;
	unsigned int size;
} bspscq_t;

/**
 * @brief Initialize a queue.
 *
 * @param queue The queue to initialize.
 * @param values An array of pointers to be used as the queue's storage.
 *   It must have at lease @p size elements.
 * @param size The size of the queue.
 *   This must be a power of 2.
 *
 * @see bspscq_produce
 * @see bspscq_consume
 */
BSPSCQ_API void
bspscq_init(bspscq_t* queue, void** values, unsigned int size);

/**
 * @brief Clean up a queue.
 *
 * All operations on the queue after this will be undefined behaviour.
 * The user is responsible for stopping both the producer and the consumer
 * before this is called.
 * Usually, this can be done by sending the consumer a "stop" message through
 * the queue and join with its thread.
 *
 * @param queue The queue to clean up.
 */
BSPSCQ_API void
bspscq_cleanup(bspscq_t* queue);

/**
 * @brief Put an item into the queue.
 *
 * @param queue The queue to produce into.
 * @param item Pointer to the item.
 * @param wait Whether the caller will be blocked if the queue is full.
 *   The caller will be unblocked once at least one item has been consumed.
 * @return Whether the item was successfully put into the queue.
 *   If @p wait is true, this will always be true.
 *   If @p wait is false, this may return false if the queue is already full.
 */
BSPSCQ_API bool
bspscq_produce(bspscq_t* queue, void* item, bool wait);

/**
 * @brief Get an item from the queue.
 * @param queue The queue to produce into.
 * @param wait Whether the caller will be blocked if the queue is not empty.
 *   The caller will be unblocked once at least one item has been produced.
 * @return An item from the queue or NULL if the queue is empty.
 *   If @p wait is true, this will never be NULL.
 *   If @p wait is false, this may return NULL if the queue is empty.
 */
BSPSCQ_API void*
bspscq_consume(bspscq_t* queue, bool wait);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSPSCQ_IMPLEMENTATION)
#define BSPSCQ_IMPLEMENTATION
#endif

#ifdef BSPSCQ_IMPLEMENTATION

#include <assert.h>
#include <stdint.h>

static inline uint32_t
bspscq_next_pow2(uint32_t v) {
    uint32_t next = v;
    next--;
    next |= next >> 1;
    next |= next >> 2;
    next |= next >> 4;
    next |= next >> 8;
    next |= next >> 16;
    next++;

    return next;
}

static void
bspscq_signal_init(bspscq_signal_t* signal) {
	mtx_init(&signal->mtx, mtx_plain);
	cnd_init(&signal->cnd);
}

static void
bspscq_signal_cleanup(bspscq_signal_t* signal) {
	cnd_destroy(&signal->cnd);
	mtx_destroy(&signal->mtx);
}

static void
bspscq_signal_raise(bspscq_signal_t* signal) {
	mtx_lock(&signal->mtx);
	cnd_signal(&signal->cnd);
	mtx_unlock(&signal->mtx);
}

void
bspscq_init(bspscq_t* queue, void** values, unsigned int size) {
	bspscq_signal_init(&queue->can_produce);
	bspscq_signal_init(&queue->can_consume);
	queue->values = values;
	atomic_store(&queue->head, 0);
	atomic_store(&queue->tail, 0);
	atomic_store(&queue->count, 0);

#ifndef NDEBUG
	assert((size == bspscq_next_pow2(size)) && "size must be a power of 2");
#endif

	queue->size = size;
}

void
bspscq_cleanup(bspscq_t* queue) {
	bspscq_signal_cleanup(&queue->can_consume);
	bspscq_signal_cleanup(&queue->can_produce);
}

bool
bspscq_produce(bspscq_t* queue, void* item, bool wait) {
	if (atomic_load(&queue->count) == (int)queue->size) {
		if (!wait) { return false; }

		mtx_lock(&queue->can_produce.mtx);
		while (queue->count == (int)queue->size) {
			cnd_wait(&queue->can_produce.cnd, &queue->can_produce.mtx);
		}
		mtx_unlock(&queue->can_produce.mtx);
	}

	unsigned int tail = atomic_fetch_add(&queue->tail, 1);
	queue->values[tail & (queue->size - 1)] = item;
	if (atomic_fetch_add(&queue->count, 1) == 0) {
		bspscq_signal_raise(&queue->can_consume);
	}

	return true;
}

void*
bspscq_consume(bspscq_t* queue, bool wait) {
	if (atomic_load(&queue->count) == 0) {
		if (!wait) { return NULL; }

		mtx_lock(&queue->can_consume.mtx);
		while (queue->count == 0) {
			cnd_wait(&queue->can_consume.cnd, &queue->can_consume.mtx);
		}
		mtx_unlock(&queue->can_consume.mtx);
	}

	unsigned int head = atomic_fetch_add(&queue->head, 1);
	void* item = queue->values[head & (queue->size - 1)];
	if (atomic_fetch_add(&queue->count, -1) == (int)queue->size) {
		bspscq_signal_raise(&queue->can_produce);
	}

	return item;
}

#endif
