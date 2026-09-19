// The message registrations live in this unit
#define BENT_DEFINE_COMPONENTS
#include "shared.h"
#include "../../btest.h"

BENT_MSG(hit_msg) { bent_t source; int amount; };
BENT_MSG(echo_msg) { int depth; double value; };
BENT_MSG(unused_msg) { int x; };
BENT_MSG(tick_msg) { int turn; };

// Nobody but the systems below know about these
BENT_TAG_COMP(chatty)
BENT_TAG_COMP(loud)

typedef struct {
	const char* sys;
	const char* msg;
	bent_t entity;
	int amount;
	int depth;
	double value;
} event_t;

static struct {
	event_t events[2048];
	int num_events;
} log;

static void
init_per_message_test(void) {
	memset(&log, 0, sizeof(log));
	init_per_test();
}

static btest_suite_t message = {
	.name = "bent/message",
	.init_per_test = init_per_message_test,
	.cleanup_per_test = cleanup_per_test,
};

static void
record(const char* sys, const char* msg, bent_t entity, int amount, int depth, double value) {
	BENT_ASSERT(log.num_events < (int)(sizeof(log.events) / sizeof(log.events[0])));
	log.events[log.num_events++] = (event_t){
		.sys = sys,
		.msg = msg,
		.entity = entity,
		.amount = amount,
		.depth = depth,
		.value = value,
	};
}

static int
count_events(const char* sys, const char* msg) {
	int count = 0;
	for (int i = 0; i < log.num_events; ++i) {
		const event_t* event = &log.events[i];
		if (
			(sys == NULL || strcmp(event->sys, sys) == 0)
			&& (msg == NULL || strcmp(event->msg, msg) == 0)
		) {
			++count;
		}
	}
	return count;
}

// hit_sys1: basic_component, handles hit {{{

static void
hit_sys1_on_hit(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const hit_msg_t* hit = msg;
	record("hit_sys1", "hit", entity, hit->amount, 0, 0.0);
}

static void
hit_sys1_on_tick(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const tick_msg_t* tick = msg;
	record("hit_sys1", "tick", entity, tick->turn, 0, 0.0);
}

BENT_DEFINE_SYS(hit_sys1) = {
	.require = BENT_COMP_LIST(&basic_component),
	.handlers = BENT_MSG_HANDLERS(
		{ &hit_msg, hit_sys1_on_hit },
		{ &tick_msg, hit_sys1_on_tick }
	),
};

// }}}

// hit_sys2: basic_component2, handles hit and echo {{{

static void
hit_sys2_on_hit(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const hit_msg_t* hit = msg;
	record("hit_sys2", "hit", entity, hit->amount, 0, 0.0);
}

static void
hit_sys2_on_echo(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const echo_msg_t* echo = msg;
	record("hit_sys2", "echo", entity, 0, echo->depth, echo->value);
}

static void
hit_sys2_on_tick(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const tick_msg_t* tick = msg;
	record("hit_sys2", "tick", entity, tick->turn, 0, 0.0);
}

BENT_DEFINE_SYS(hit_sys2) = {
	.require = BENT_COMP_LIST(&basic_component2),
	.handlers = BENT_MSG_HANDLERS(
		{ &hit_msg, hit_sys2_on_hit },
		{ &echo_msg, hit_sys2_on_echo },
		{ &tick_msg, hit_sys2_on_tick }
	),
};

// }}}

// relay_sys: broadcasts from inside a handler {{{

static void
relay_on_tick(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const tick_msg_t* tick = msg;
	record("relay", "tick", entity, tick->turn, 0, 0.0);
	if (tick->turn == 1) {
		bent_broadcast(world, tick_msg, { .turn = 2 });
	}
}

BENT_DEFINE_SYS(relay_sys) = {
	.require = BENT_COMP_LIST(&basic_component),
	.handlers = BENT_MSG_HANDLERS(
		{ &tick_msg, relay_on_tick }
	),
};

// }}}

// chatty_sys: sends from inside its handlers {{{

typedef struct {
	// Number of echoes sent per hit, at least 1
	int fanout;
	// Remove basic_component2 from the entity after sending the echoes
	bool remove_comp2;
} chatty_sys_t;

static void
chatty_on_hit(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	chatty_sys_t* sys = userdata;
	const hit_msg_t* hit = msg;
	record("chatty", "hit", entity, hit->amount, 0, 0.0);

	int fanout = sys->fanout > 0 ? sys->fanout : 1;
	for (int i = 0; i < fanout; ++i) {
		// Built on this stack frame: it has to be copied to survive
		echo_msg_t echo = bent_msg(echo_msg){ .depth = 1, .value = hit->amount + i * 0.5 };
		bent_send(world, entity, echo_msg, echo);
	}

	if (sys->remove_comp2) {
		bent_remove(world, entity, basic_component2);
	}
}

static void
chatty_on_echo(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const echo_msg_t* echo = msg;
	record("chatty", "echo", entity, 0, echo->depth, echo->value);
	if (echo->depth < 3) {
		bent_send(world, entity, echo_msg, { .depth = echo->depth + 1, .value = echo->value });
	}
}

BENT_DEFINE_SYS(chatty_sys) = {
	.size = sizeof(chatty_sys_t),
	.require = BENT_COMP_LIST(&chatty),
	.handlers = BENT_MSG_HANDLERS(
		{ &hit_msg, chatty_on_hit },
		{ &echo_msg, chatty_on_echo }
	),
};

// }}}

// loud_sys: sends from its add callback {{{

static void
loud_add(void* userdata, bent_world_t* world, bent_t entity) {
	record("loud", "add", entity, 0, 0, 0.0);
	bent_send(world, entity, hit_msg, { .source = entity, .amount = 7 });
	record("loud", "add_done", entity, 0, 0, 0.0);
}

static void
loud_on_hit(void* userdata, bent_world_t* world, bent_t entity, const void* msg) {
	const hit_msg_t* hit = msg;
	record("loud", "hit", entity, hit->amount, 0, 0.0);
}

BENT_DEFINE_SYS(loud_sys) = {
	.require = BENT_COMP_LIST(&loud),
	.add = loud_add,
	.handlers = BENT_MSG_HANDLERS(
		{ &hit_msg, loud_on_hit }
	),
};

// }}}

BTEST(message, delivered_to_matching_systems) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	// Matches nothing yet
	bent_send(world, ent, hit_msg, { .amount = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 0);

	bent_add(world, ent, basic_component, NULL);
	bent_send(world, ent, hit_msg, { .source = ent, .amount = 2 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 1);
	BTEST_EXPECT(strcmp(log.events[0].sys, "hit_sys1") == 0);
	BTEST_EXPECT(strcmp(log.events[0].msg, "hit") == 0);
	BTEST_EXPECT(bent_equal(log.events[0].entity, ent));
	BTEST_EXPECT_EQUAL("%d", log.events[0].amount, 2);

	bent_add(world, ent, basic_component2, NULL);
	bent_send(world, ent, hit_msg, { .amount = 3 });
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys1", "hit"), 2);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "hit"), 1);
	BTEST_EXPECT_EQUAL("%d", log.num_events, 3);

	// A message nobody handles
	bent_send(world, ent, unused_msg, { .x = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 3);

	// Only systems that handle the type are called, matching is not enough
	bent_send(world, ent, echo_msg, { .depth = 1 });
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "echo"), 1);
	BTEST_EXPECT_EQUAL("%d", log.num_events, 4);

	bent_destroy(world, ent);
	bent_send(world, ent, hit_msg, { .amount = 4 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 4);
}

BTEST(message, value_form) {
	bent_world_t* world = fixture.world;

	bent_t a = bent_create(world);
	bent_t b = bent_create(world);
	bent_add(world, a, basic_component, NULL);
	bent_add(world, b, basic_component2, NULL);

	// The same message sent to both sides of a pair
	hit_msg_t hit = bent_msg(hit_msg){ .source = a, .amount = 9 };
	bent_send(world, a, hit_msg, hit);
	bent_send(world, b, hit_msg, hit);

	BTEST_EXPECT_EQUAL("%d", log.num_events, 2);
	BTEST_EXPECT(strcmp(log.events[0].sys, "hit_sys1") == 0);
	BTEST_EXPECT(bent_equal(log.events[0].entity, a));
	BTEST_EXPECT(strcmp(log.events[1].sys, "hit_sys2") == 0);
	BTEST_EXPECT(bent_equal(log.events[1].entity, b));
	BTEST_EXPECT_EQUAL("%d", log.events[1].amount, 9);
}

BTEST(message, nested_sends_are_queued) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component2, NULL);
	bent_add(world, ent, chatty, NULL);

	// The hit: hit_sys2 and chatty_sys.
	// Queued: echo depth 1 -> hit_sys2 and chatty_sys, which queues depth 2, etc.
	bent_send(world, ent, hit_msg, { .amount = 4 });

	BTEST_EXPECT_EQUAL("%d", count_events(NULL, "hit"), 2);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "echo"), 3);
	BTEST_EXPECT_EQUAL("%d", count_events("chatty", "echo"), 3);
	BTEST_EXPECT_EQUAL("%d", log.num_events, 8);

	// Every handler of a message runs before anything it queued
	BTEST_EXPECT(strcmp(log.events[0].msg, "hit") == 0);
	BTEST_EXPECT(strcmp(log.events[1].msg, "hit") == 0);
	for (int i = 2; i < log.num_events; ++i) {
		const event_t* event = &log.events[i];
		BTEST_EXPECT(strcmp(event->msg, "echo") == 0);
		BTEST_EXPECT_EQUAL("%d", event->depth, (i - 2) / 2 + 1);
		// The payload was copied from the sender's stack frame
		BTEST_EXPECT_EQUAL("%f", event->value, 4.0);
	}
}

BTEST(message, many_nested_sends) {
	bent_world_t* world = fixture.world;
	chatty_sys_t* chatty_data = bent_get_sys_data(world, chatty_sys);
	chatty_data->fanout = 100;

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component2, NULL);
	bent_add(world, ent, chatty, NULL);

	bent_send(world, ent, hit_msg, { .amount = 1 });

	// 100 echoes per depth, 3 depths, 2 systems each
	BTEST_EXPECT_EQUAL("%d", count_events(NULL, "echo"), 100 * 3 * 2);

	// Queued in rounds: all of depth 1, then all of depth 2, then 3
	// Within a round, in the order they were sent
	int index = 2;
	for (int depth = 1; depth <= 3; ++depth) {
		for (int i = 0; i < 100; ++i) {
			for (int sys = 0; sys < 2; ++sys) {
				const event_t* event = &log.events[index++];
				BTEST_EXPECT_EQUAL("%d", event->depth, depth);
				BTEST_EXPECT_EQUAL("%f", event->value, 1.0 + i * 0.5);
			}
		}
	}
}

BTEST(message, queued_send_uses_current_membership) {
	bent_world_t* world = fixture.world;
	chatty_sys_t* chatty_data = bent_get_sys_data(world, chatty_sys);
	chatty_data->remove_comp2 = true;

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component2, NULL);
	bent_add(world, ent, chatty, NULL);

	bent_send(world, ent, hit_msg, { .amount = 1 });

	// The first echo was sent before the removal so it is delivered before
	// hit_sys2 is told about it.
	// The rest come after and are dropped.
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "hit"), 1);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "echo"), 1);
	BTEST_EXPECT_EQUAL("%d", count_events("chatty", "echo"), 3);
}

BTEST(message, send_from_add_callback_is_queued) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);
	bent_add(world, ent, loud, NULL);

	// The add callback ran to completion before the message went out
	BTEST_EXPECT_EQUAL("%d", log.num_events, 4);
	BTEST_EXPECT(strcmp(log.events[0].msg, "add") == 0);
	BTEST_EXPECT(strcmp(log.events[1].msg, "add_done") == 0);
	BTEST_EXPECT_EQUAL("%d", count_events("loud", "hit"), 1);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys1", "hit"), 1);
	BTEST_EXPECT_EQUAL("%d", log.events[2].amount, 7);
	BTEST_EXPECT_EQUAL("%d", log.events[3].amount, 7);
}

BTEST(message, not_delivered_while_loading) {
	bent_world_t* world = fixture.world;

	bent_begin_load(world);
	bent_t ent = { .index = 0, .gen = 1 };
	BTEST_EXPECT(bent_reserve(world, ent));
	bent_restore(world, ent, basic_component);

	bent_send(world, ent, hit_msg, { .amount = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 0);

	bent_end_load(world);
	bent_send(world, ent, hit_msg, { .amount = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 1);
}

BTEST(message, no_handlers) {
	bent_world_t* world = fixture.world;

	// double_match_system has no handlers and must be skipped
	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);
	bent_add(world, ent, basic_component2, NULL);
	BTEST_EXPECT(bent_match(world, double_match_system, ent));

	bent_send(world, ent, unused_msg, { .x = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 0);
}

BTEST(message, broadcast_reaches_every_handler) {
	bent_world_t* world = fixture.world;

	// No entity exists, matching is not involved
	bent_broadcast(world, tick_msg, { .turn = 5 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 3);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys1", "tick"), 1);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "tick"), 1);
	BTEST_EXPECT_EQUAL("%d", count_events("relay", "tick"), 1);
	// In registration order, with an invalid entity
	BTEST_EXPECT(strcmp(log.events[0].sys, "hit_sys1") == 0);
	BTEST_EXPECT(strcmp(log.events[1].sys, "hit_sys2") == 0);
	BTEST_EXPECT(strcmp(log.events[2].sys, "relay") == 0);
	for (int i = 0; i < log.num_events; ++i) {
		BTEST_EXPECT(bent_is_invalid(log.events[i].entity));
		BTEST_EXPECT_EQUAL("%d", log.events[i].amount, 5);
	}

	// Only systems that handle the type are called
	bent_broadcast(world, unused_msg, { .x = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 3);

	// The value form
	tick_msg_t tick = bent_msg(tick_msg){ .turn = 6 };
	bent_broadcast(world, tick_msg, tick);
	BTEST_EXPECT_EQUAL("%d", log.num_events, 6);
	BTEST_EXPECT_EQUAL("%d", log.events[log.num_events - 1].amount, 6);
}

BTEST(message, broadcast_ignores_matching) {
	bent_world_t* world = fixture.world;

	// Entities that match some handlers' systems and not others change nothing
	bent_t ent = bent_create(world);
	bent_add(world, ent, basic_component, NULL);
	bent_broadcast(world, tick_msg, { .turn = 5 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 3);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "tick"), 1);
	BTEST_EXPECT(bent_is_invalid(log.events[0].entity));

	// And a send to the entity still matches as usual: hit_sys1 and relay
	bent_send(world, ent, tick_msg, { .turn = 7 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 5);
	BTEST_EXPECT_EQUAL("%d", count_events("hit_sys2", "tick"), 1);
	BTEST_EXPECT(bent_equal(log.events[log.num_events - 1].entity, ent));
}

BTEST(message, nested_broadcast_is_queued) {
	bent_world_t* world = fixture.world;

	// Turn 1 goes to the three handlers, the relay broadcasts turn 2 from
	// inside its handler: queued, then delivered to all three once the outer
	// delivery is done
	bent_broadcast(world, tick_msg, { .turn = 1 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 6);
	for (int i = 0; i < 3; ++i) {
		BTEST_EXPECT_EQUAL("%d", log.events[i].amount, 1);
		BTEST_EXPECT_EQUAL("%d", log.events[i + 3].amount, 2);
	}
	BTEST_EXPECT_EQUAL("%d", count_events("relay", "tick"), 2);
}

BTEST(message, broadcast_not_delivered_while_loading) {
	bent_world_t* world = fixture.world;

	bent_begin_load(world);
	bent_broadcast(world, tick_msg, { .turn = 4 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 0);
	bent_end_load(world);

	bent_broadcast(world, tick_msg, { .turn = 4 });
	BTEST_EXPECT_EQUAL("%d", log.num_events, 3);
}
