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

/*! An internal signal, should be treated as opaque */
typedef struct {
	/// @cond INTERNAL
	mtx_t mtx;
	cnd_t cnd;
	/// @endcond
} bspscq_signal_t;

/*! A single producer single consumer queue, should be treated as opaque */
typedef struct bspscq_s {
	/// @cond INTERNAL
	bspscq_signal_t can_produce;
	bspscq_signal_t can_consume;
	void** values;
	unsigned int size;
	// Each hot field lives on its own cache line to avoid false sharing.
	_Alignas(64) atomic_uint count;
	_Alignas(64) unsigned int head;
	_Alignas(64) unsigned int tail;
	/// @endcond
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
 *
 * @param queue The queue to consume from.
 * @param itemp Where the consumed item will be stored.
 *   This is only written to when the function returns true.
 * @param wait Whether the caller will be blocked if the queue is empty.
 *   The caller will be unblocked once at least one item has been produced.
 * @return Whether an item was successfully taken out of the queue.
 *   If @p wait is true, this will always be true.
 *   If @p wait is false, this may return false if the queue is empty.
 */
BSPSCQ_API bool
bspscq_consume(bspscq_t* queue, void** itemp, bool wait);

#endif

#if defined(BLIB_IMPLEMENTATION) && !defined(BSPSCQ_IMPLEMENTATION)
#define BSPSCQ_IMPLEMENTATION
#endif

#ifdef BSPSCQ_IMPLEMENTATION

#include <assert.h>

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
	queue->head = 0;
	queue->tail = 0;
	atomic_store(&queue->count, 0);

	assert((size != 0) && ((size & (size - 1)) == 0) && "size must be a power of 2");

	queue->size = size;
}

void
bspscq_cleanup(bspscq_t* queue) {
	bspscq_signal_cleanup(&queue->can_consume);
	bspscq_signal_cleanup(&queue->can_produce);
}

bool
bspscq_produce(bspscq_t* queue, void* item, bool wait) {
	if (atomic_load_explicit(&queue->count, memory_order_acquire) == queue->size) {
		if (!wait) { return false; }

		mtx_lock(&queue->can_produce.mtx);
		while (atomic_load_explicit(&queue->count, memory_order_acquire) == queue->size) {
			cnd_wait(&queue->can_produce.cnd, &queue->can_produce.mtx);
		}
		mtx_unlock(&queue->can_produce.mtx);
	}

	unsigned int tail = queue->tail++;
	queue->values[tail & (queue->size - 1)] = item;
	if (atomic_fetch_add_explicit(&queue->count, 1, memory_order_release) == 0) {
		bspscq_signal_raise(&queue->can_consume);
	}

	return true;
}

bool
bspscq_consume(bspscq_t* queue, void** itemp, bool wait) {
	if (atomic_load_explicit(&queue->count, memory_order_acquire) == 0) {
		if (!wait) { return false; }

		mtx_lock(&queue->can_consume.mtx);
		while (atomic_load_explicit(&queue->count, memory_order_acquire) == 0) {
			cnd_wait(&queue->can_consume.cnd, &queue->can_consume.mtx);
		}
		mtx_unlock(&queue->can_consume.mtx);
	}

	unsigned int head = queue->head++;
	*itemp = queue->values[head & (queue->size - 1)];
	if (atomic_fetch_sub_explicit(&queue->count, 1, memory_order_release) == queue->size) {
		bspscq_signal_raise(&queue->can_produce);
	}

	return true;
}

#endif
