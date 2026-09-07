/*
 * Copyright (c) 2016 National Cheng Kung University, Taiwan.
 * All rights reserved.
 * Use of this source code is governed by a BSD-style license.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "../../btest.h"

// MSVC's rand does not seem to be able to finish the random test
#define RND_IMPLEMENTATION
#include "rnd.h"

#define TLSF_IMPLEMENTATION
#define TLSF_ENABLE_CHECK
#include "../../tlsf.h"

#define rand() rnd_well_next(&rnd_state)

static size_t PAGE;
static size_t MAX_PAGES;
static rnd_well_t rnd_state;
static tlsf_t t;

static void
init_per_test(void) {
#ifdef __linux__
    PAGE = (size_t) sysconf(_SC_PAGESIZE);
#else
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    PAGE = sys_info.dwPageSize;
#endif

    MAX_PAGES = 20 * TLSF_MAX_SIZE / PAGE;
    tlsf_init(&t, MAX_PAGES * PAGE);
}

static void
cleanup_per_test(void) {
    tlsf_cleanup(&t);
}

static btest_suite_t tlsf_ = {
    .name = "tlsf",
    .init_per_test = init_per_test,
    .cleanup_per_test = cleanup_per_test,
};

static void random_test(tlsf_t *t, size_t spacelen, const size_t cap)
{
    const size_t maxitems = 2 * spacelen;

    void **p = (void **) malloc(maxitems * sizeof(void *));
    BTEST_ASSERT(p);

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
                BTEST_ASSERT(!((size_t) p[i] % align));
        }
        BTEST_ASSERT(p[i]);
        rest -= (int64_t) len;

        if (rand() % 10 == 0) {
            len = ((size_t) rand() % cap) + 1;
            p[i] = tlsf_realloc(t, p[i], len);
            BTEST_ASSERT(p[i]);
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
        BTEST_ASSERT(data[0] == 0xa5);
        tlsf_free(t, p[target]);
        p[target] = NULL;
        n--;

        tlsf_check(t);
    }

    free(p);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

BTEST(tlsf_, random_sizes)
{
    unsigned int seed = (unsigned int)time(0);
    BLOG_INFO("seed = %u", seed);
    rnd_well_seed(&rnd_state, seed);

    const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 1024 * 1024};

    for (unsigned i = 0; i < ARRAY_SIZE(sizes); i++) {
        unsigned n = 1024;

        while (n--) {
            size_t cap = (size_t) rand() % sizes[i] + 1;
            random_test(&t, sizes[i], cap);
        }
    }
}

#ifdef __linux__

static void large_alloc(tlsf_t *t, size_t s)
{
    for (size_t d = 0; d < 100 && d < s; ++d) {
        void *p = tlsf_malloc(t, s - d);
        BTEST_ASSERT(p);

        void *q = tlsf_malloc(t, s - d);
        BTEST_ASSERT(q);
        tlsf_free(t, q);

        q = tlsf_malloc(t, s - d);
        BTEST_ASSERT(q);
        tlsf_free(t, q);

        tlsf_free(t, p);
        tlsf_check(t);
    }
}

BTEST(tlsf_, large_sizes)
{
    size_t s = 1;
    while (s <= TLSF_MAX_SIZE) {
        large_alloc(&t, s);
        s *= 2;
    }

    s = TLSF_MAX_SIZE;
    while (s > 0) {
        large_alloc(&t, s);
        s /= 2;
    }
}

#endif
