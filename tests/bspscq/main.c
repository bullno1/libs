#include "../../bspscq.h"
#include "../../btest.h"

typedef struct {
	bool stop;
	int content;
} message_t;

typedef struct {
	bspscq_t* req;
	bspscq_t* res;
	// The worker cannot use the (longjmp based) assertions of the test
	// framework so it records failures for the test thread to check
	bool failed;
} worker_context_t;

static btest_suite_t bspscq_ = {
	.name = "bspscq",
};

static int
worker_thread_entry(void* arg) {
	worker_context_t* ctx = arg;

	while (true) {
		void* item;
		if (!bspscq_consume(ctx->req, &item, true)) {
			ctx->failed = true;
			return -1;
		}
		message_t* message = item;
		bool should_stop = message->stop;
		int content = message->content;
		if (!bspscq_produce(ctx->res, message, true)) {
			ctx->failed = true;
			return -1;
		}

		if (should_stop) { return content; }
	}
}

BTEST(bspscq_, request_response) {
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

	BTEST_ASSERT_EQUAL("%d", thrd_create(&thread, worker_thread_entry, &ctx), thrd_success);

	for (int i = 0; i < 5; ++i) {
		message_t* msg = &messages[(message_index++) % 6];
		msg->content = i;
		msg->stop = false;
		BTEST_EXPECT(bspscq_produce(&requests, msg, true));
	}
	message_t* stop_msg = &messages[(message_index++) % 6];
	stop_msg->stop = true;
	stop_msg->content = 69;
	BTEST_EXPECT(bspscq_produce(&requests, stop_msg, true));

	for (int i = 0; i < 5; ++i) {
		void* item;
		BTEST_EXPECT(bspscq_consume(&responses, &item, true));
		message_t* msg = item;
		BTEST_EXPECT_EQUAL("%d", msg->content, i);
	}
	void* stop_response;
	BTEST_EXPECT(bspscq_consume(&responses, &stop_response, true));
	BTEST_EXPECT(stop_response == stop_msg);

	int res;
	thrd_join(thread, &res);
	BTEST_EXPECT_EQUAL("%d", res, 69);
	BTEST_EXPECT(!ctx.failed);

	bspscq_cleanup(&responses);
	bspscq_cleanup(&requests);
}

#define BLIB_IMPLEMENTATION
#include "../../bspscq.h"
