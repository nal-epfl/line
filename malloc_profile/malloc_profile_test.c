/*
 *	Copyright (C) 2014 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// gcc -O2 -Wall -fPIC -std=gnu99 -o malloc_profile_test malloc_profile_test.c -ldl -lrt
// LD_PRELOAD="$(pwd)/malloc_profile.so" ./malloc_profile_test

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "malloc_profile_wrapper.h"

static char* allocate(int size)
{
    char *p = malloc(size);
    if (p) {
        for (int i = 0; i < size; i++) {
            p[i] = 0;
        }
    }
    return p;
}

size_t make_size(int i)
{
    return (1 << i) * 1024;
}

int main()
{
    const int actual_num_allocs = 10;

    malloc_profile_reset_wrapper();

    for (int i = 0; i < actual_num_allocs; i++) {
        allocate(make_size(i));
    }

    malloc_profile_pause_wrapper();

    const long long measured_num_allocs = malloc_profile_get_alloc_count_wrapper();

    fprintf(stdout, "Allocations: real %d, measured %lld\n", actual_num_allocs, measured_num_allocs);
    assert(actual_num_allocs == measured_num_allocs);

    for (int i = 0; i < actual_num_allocs; i++) {
        size_t size = make_size(i);
        const long long actual_allocs = 1;
        const long long measured_allocs = malloc_profile_get_alloc_count_by_size_wrapper(size);
        fprintf(stdout, "Allocations of size %lu: real %lld, measured %lld\n", size, actual_allocs, measured_allocs);
        assert(actual_allocs == measured_allocs);
    }

    fprintf(stdout, "All tests have passed.\n");

	fprintf(stdout, "Checking backtrace support (you should see one now)...\n");

	malloc_profile_resume_wrapper();

	malloc_profile_set_trace_cpu_wrapper(1000);
	allocate(900);
	allocate(1100);

	malloc_profile_pause_wrapper();

    return 0;
}
