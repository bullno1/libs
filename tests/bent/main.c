#include "../../bent.h"
#include "../../btest.h"
#include <string.h>

static struct {
	bent_world_t* world;
} fixture;

static void
init_per_test(void) {
	memset(&fixture, 0, sizeof(fixture));
	bent_init(&fixture.world, NULL);
}

static void
cleanup_per_test(void) {
	bent_cleanup(&fixture.world);
}

static btest_suite_t bent = {
	.name = "bent",
	.init_per_test = init_per_test,
	.cleanup_per_test = cleanup_per_test,
};

BENT_DEFINE_COMP(basic_component) = {
	.size = sizeof(int),
};

BENT_DEFINE_COMP(basic_component2) = {
	.size = sizeof(float),
};

BTEST(bent, component_basic) {
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

BTEST(bent, component_add_remove) {
	bent_world_t* world = fixture.world;

	bent_t ent = bent_create(world);

	bent_add(world, ent, basic_component, NULL);
	BTEST_EXPECT(bent_has(world, ent, basic_component));

	bent_remove(world, ent, basic_component);
	BTEST_EXPECT(!bent_has(world, ent, basic_component));
}

BTEST(bent, component_data_separate_storage) {
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

BTEST(bent, component_data_recycle_storage) {
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

BTEST(bent, null_handle) {
	bent_world_t* world = fixture.world;

	bent_t null = { 0 };
	BTEST_EXPECT(!bent_is_active(world, null));
	BTEST_EXPECT(!bent_has(world, null, basic_component));
	BTEST_EXPECT(!bent_has(world, null, basic_component2));
	BTEST_EXPECT_EQUAL("%p", bent_add(world, null, basic_component2, NULL), NULL);
	BTEST_EXPECT_EQUAL("%p", bent_get(world, null, basic_component2), NULL);

	bent_destroy(world, null);
}

#define BLIB_IMPLEMENTATION
#include "../../bent.h"
