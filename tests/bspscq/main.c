#define BLIB_IMPLEMENTATION
#include "../../bspscq.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
	bool stop;
	int content;
} message_t;

typedef struct {
	bspscq_t* req;
	bspscq_t* res;
} worker_context_t;

int worker_thread_entry(void* arg) {
	worker_context_t* ctx = arg;

	while (true) {
		message_t* message = bspscq_consume(ctx->req, true);
		assert(message != NULL);
		bool should_stop = message->stop;
		int content = message->content;
		assert(bspscq_produce(ctx->res, message, true));

		if (should_stop) { return content; }
	}

	return 0;
}

int main(int argc, const char* argv[]) {
	(void)argc;
	(void)argv;

	bspscq_t requests;
	bspscq_t responses;

	void* req_q[4];
	void* res_q[4];

	message_t messages[6];
	int message_index = 0;

	thrd_t thread;

	bspscq_init(&requests, req_q, sizeof(req_q) / sizeof(req_q[0]));
	bspscq_init(&responses, res_q, sizeof(res_q) / sizeof(res_q[0]));

	worker_context_t ctx = {
		.req = &requests,
		.res = &responses,
	};

	thrd_create(&thread, worker_thread_entry, &ctx);

	for (int i = 0; i < 5; ++i) {
		message_t* msg = &messages[(message_index++) % 6];
		msg->content = i;
		msg->stop = false;
		assert(bspscq_produce(&requests, msg, true));
	}
	message_t* stop_msg = &messages[(message_index++) % 6];
	stop_msg->stop = true;
	stop_msg->content = 69;
	assert(bspscq_produce(&requests, stop_msg, true));

	for (int i = 0; i < 5; ++i) {
		message_t* msg = bspscq_consume(&responses, true);
		assert(msg != NULL);
		assert(msg->content == i);
	}
	message_t* stop_response = bspscq_consume(&responses, true);
	assert(stop_response == stop_msg);

	int res;
	thrd_join(thread, &res);
	assert(res == 69);

	bspscq_cleanup(&responses);
	bspscq_cleanup(&requests);

	return 0;
}
