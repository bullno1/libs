#include "shared.h"
#include "../../btest.h"

static btest_suite_t component = {
	.name = "bent/component",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

BTEST(component, basic) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);
	BTEST_EXPECT(!bent_has(world, ent, basic_component));
	BTEST_EXPECT(bent_is_active(world, ent));

	void* ptr1 = bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT(ptr1 != NULL);
	void* ptr2 = bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT_EQUAL("%p", ptr1, ptr2);

	BTEST_EXPECT(bent_has(world, ent, basic_component));

	bent_destroy(world, ent);
	BTEST_EXPECT(!bent_has(world, ent, basic_component));
	BTEST_EXPECT(!bent_is_active(world, ent));
}

BTEST(component, add_remove) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT(bent_has(world, ent, basic_component));

	bent_remove(world, ent, basic_component);
	BTEST_EXPECT(!bent_has(world, ent, basic_component));
}

BTEST(component, data_has_separate_storage) {
	bent_world_t* world = fixture.world;

	bent_t ent1 = bent_create(world);
	bent_t ent2 = bent_create(world);

	bent_add(world, ent1, basic_component, &(int){ 1 });
	bent_add(world, ent2, basic_component, &(int){ 2 });

	int data1 = *(int*)bent_get(world, ent1, basic_component);
	int data2 = *(int*)bent_get(world, ent2, basic_component);

	BTEST_EXPECT_EQUAL("%d", data1, 1);
	BTEST_EXPECT_EQUAL("%d", data2, 2);

	bent_destroy(world, ent1);
	data2 = *(int*)bent_get(world, ent2, basic_component);
	BTEST_EXPECT_EQUAL("%d", data2, 2);

	bent_t ent3 = bent_create(world);

	BTEST_EXPECT(!bent_has(world, ent3, basic_component));
	BTEST_EXPECT(!bent_has(world, ent3, basic_component2));

	bent_add(world, ent3, basic_component2, &(float) { 6.7f });

	BTEST_EXPECT(!bent_has(world, ent3, basic_component));
	BTEST_EXPECT(bent_has(world, ent3, basic_component2));

	data2 = *(int*)bent_get(world, ent2, basic_component);
	float data3 = *(float*)bent_get(world, ent3, basic_component2);

	BTEST_EXPECT_EQUAL("%d", data2, 2);
	BTEST_EXPECT_EQUAL("%f", data3, 6.7f);
}

BTEST(component, data_recycle_storage) {
	bent_world_t* world = fixture.world;

	bent_t ent1 = bent_create(world);
	bent_t ent2 = bent_create(world);
	(void)ent2;

	bent_destroy(world, ent1);
	bent_t ent3 = bent_create(world);

	BTEST_EXPECT_EQUAL("%d", ent1.index, ent3.index);
	BTEST_EXPECT(bent_is_active(world, ent3));
	BTEST_EXPECT(!bent_is_active(world, ent1));
}

BTEST(component, null_handle) {
	bent_world_t* world = fixture.world;

	bent_t null = { 0 };
	BTEST_EXPECT(!bent_is_active(world, null));
	BTEST_EXPECT(!bent_has(world, null, basic_component));
	BTEST_EXPECT(!bent_has(world, null, basic_component2));
	BTEST_EXPECT_EQUAL("%p", bent_add(world, null, basic_component2, NULL), NULL);
	BTEST_EXPECT_EQUAL("%p", bent_get(world, null, basic_component2), NULL);

	bent_destroy(world, null);
}

typedef struct component_with_callback_init_s component_with_callback_init_t;

typedef struct {
	component_with_callback_init_t* init_arg;
} component_with_callback_t;

struct component_with_callback_init_s {
	int value;
};

static void
component_with_callback_init(void* data_, void* arg) {
	component_with_callback_t* data = data_;
	data->init_arg = arg;
	data->init_arg->value = 1;
}

static void
component_with_callback_cleanup(void* data_) {
	component_with_callback_t* data = data_;
	data->init_arg->value = 2;
}

BENT_DEFINE_COMP(component_with_callback) = {
	.size = sizeof(component_with_callback_t),
	.init = component_with_callback_init,
	.cleanup = component_with_callback_cleanup,
};
BENT_DEFINE_COMP_ADDER_EX(component_with_callback, component_with_callback_t, component_with_callback_init_t)

BTEST(component, callback) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	component_with_callback_init_t arg = { .value = 3 };
	component_with_callback_t* comp = bent_add_component_with_callback(world, ent, &arg);
	BTEST_EXPECT_EQUAL("%p", (void*)comp->init_arg, (void*)&arg);
	BTEST_EXPECT_EQUAL("%d", arg.value, 1);

	// Remove should invoke the callback
	bent_remove(world, ent, component_with_callback);
	BTEST_EXPECT_EQUAL("%d", arg.value, 2);

	// Add again to init again
	bent_add_component_with_callback(world, ent, &arg);
	BTEST_EXPECT_EQUAL("%d", arg.value, 1);

	// Destroy should invoke the callback
	bent_destroy(world, ent);
	BTEST_EXPECT_EQUAL("%d", arg.value, 2);
}
