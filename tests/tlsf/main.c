/*
 * Copyright (c) 2016 National Cheng Kung University, Taiwan.
 * All rights reserved.
 * Use of this source code is governed by a BSD-style license.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

// MSVC's rand does not seem to be able to finish the random test
#define RND_IMPLEMENTATION
#include "rnd.h"

#define TLSF_IMPLEMENTATION
#define TLSF_ENABLE_CHECK
#define BARENA_IMPLEMENTATION
#include "../../tlsf.h"

#define rand() rnd_well_next(&rnd_state)

static size_t PAGE;
static size_t MAX_PAGES;
static rnd_well_t rnd_state;

static void random_test(tlsf_t *t, size_t spacelen, const size_t cap)
{
    const size_t maxitems = 2 * spacelen;

    void **p = (void **) malloc(maxitems * sizeof(void *));
    assert(p);

    /* Allocate random sizes up to the cap threshold.
     * Track them in an array.
     */
    int64_t rest = (int64_t) spacelen * (rand() % 6 + 1);
    unsigned i = 0;
    while (rest > 0) {
        size_t len = ((size_t) rand() % cap) + 1;
        if (rand() % 2 == 0) {
            p[i] = tlsf_malloc(t, len);
        } else {
            size_t align = 1ull << (rand() % 20);
            if (cap < align)
                align = 0;
            else
                len = align * (((size_t) rand() % (cap / align)) + 1);
            p[i] = !align || !len ? tlsf_malloc(t, len)
                                  : tlsf_aalloc(t, align, len);
            if (align)
                assert(!((size_t) p[i] % align));
        }
        assert(p[i]);
        rest -= (int64_t) len;

        if (rand() % 10 == 0) {
            len = ((size_t) rand() % cap) + 1;
            p[i] = tlsf_realloc(t, p[i], len);
            assert(p[i]);
        }

        tlsf_check(t);

        /* Fill with magic (only when testing up to 1MB). */
        uint8_t *data = (uint8_t *) p[i];
        if (spacelen <= 1024ull * 1024)
            memset(data, 0, len);
        data[0] = 0xa5;

        if (++i == maxitems)
            break;
    }

    /* Randomly deallocate the memory blocks until all of them are freed.
     * The free space should match the free space after initialisation.
     */
    for (unsigned n = i; n;) {
        size_t target = (size_t) rand() % i;
        if (p[target] == NULL)
            continue;
        uint8_t *data = (uint8_t *) p[target];
        (void)data;
        assert(data[0] == 0xa5);
        tlsf_free(t, p[target]);
        p[target] = NULL;
        n--;

        tlsf_check(t);
    }

    free(p);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static void random_sizes_test(tlsf_t *t)
{
    const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 1024 * 1024};

    for (unsigned i = 0; i < ARRAY_SIZE(sizes); i++) {
        unsigned n = 1024;

        while (n--) {
            size_t cap = (size_t) rand() % sizes[i] + 1;
            printf("sizes = %zu, cap = %zu\n", sizes[i], cap);
            random_test(t, sizes[i], cap);
        }
    }
}

static void large_alloc(tlsf_t *t, size_t s)
{
    printf("large alloc %zu\n", s);
    for (size_t d = 0; d < 100 && d < s; ++d) {
        void *p = tlsf_malloc(t, s - d);
        assert(p);

        void *q = tlsf_malloc(t, s - d);
        assert(q);
        tlsf_free(t, q);

        q = tlsf_malloc(t, s - d);
        assert(q);
        tlsf_free(t, q);

        tlsf_free(t, p);
        tlsf_check(t);
    }
}

static void large_size_test(tlsf_t *t)
{
    size_t s = 1;
    while (s <= TLSF_MAX_SIZE) {
        large_alloc(t, s);
        s *= 2;
    }

    s = TLSF_MAX_SIZE;
    while (s > 0) {
        large_alloc(t, s);
        s /= 2;
    }
}

int main(void)
{
#ifdef __linux__
    PAGE = (size_t) sysconf(_SC_PAGESIZE);
#else
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    PAGE = sys_info.dwPageSize;
#endif

    MAX_PAGES = 20 * TLSF_MAX_SIZE / PAGE;
    tlsf_t t = TLSF_INIT;

    barena_t arena;
    barena_init(&arena, MAX_PAGES * PAGE, PAGE);
    t.userdata = &arena;

    // Windows does not do overcommit/lazy commit so a test with the entire
    // address space cannot work.
#ifdef __linux__
    large_size_test(&t);
#endif

    rnd_well_seed(&rnd_state, (unsigned int)time(0));
    random_sizes_test(&t);
    puts("OK!");

    barena_cleanup(&arena);
    return 0;
}
