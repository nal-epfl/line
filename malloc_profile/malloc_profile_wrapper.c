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

#include "malloc_profile_wrapper.h"

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>

long long malloc_profile_get_alloc_count_wrapper()
{
    long long (*malloc_profile_get_alloc_count)() = dlsym(RTLD_DEFAULT, "malloc_profile_get_alloc_count");
    if (malloc_profile_get_alloc_count) {
        return malloc_profile_get_alloc_count();
    }
    return 0;
}

long long malloc_profile_get_alloc_count_cpu_wrapper()
{
    long long (*malloc_profile_get_alloc_count_cpu)() = dlsym(RTLD_DEFAULT, "malloc_profile_get_alloc_count_cpu");
    if (malloc_profile_get_alloc_count_cpu) {
        return malloc_profile_get_alloc_count_cpu();
    }
    return 0;
}

long long malloc_profile_get_alloc_count_by_size_wrapper(size_t size)
{
    long long (*malloc_profile_get_alloc_count_by_size)(size_t) = dlsym(RTLD_DEFAULT, "malloc_profile_get_alloc_count_by_size");
    if (malloc_profile_get_alloc_count_by_size) {
        return malloc_profile_get_alloc_count_by_size(size);
    }
    return 0;
}

long long malloc_profile_get_alloc_count_cpu_by_size_wrapper(size_t size)
{
    long long (*malloc_profile_get_alloc_count_cpu_by_size)(size_t) = dlsym(RTLD_DEFAULT, "malloc_profile_get_alloc_count_cpu_by_size");
    if (malloc_profile_get_alloc_count_cpu_by_size) {
        return malloc_profile_get_alloc_count_cpu_by_size(size);
    }
    return 0;
}

void malloc_profile_set_trace_cpu_wrapper(size_t size)
{
	void (*malloc_profile_set_trace_cpu)(size_t) = dlsym(RTLD_DEFAULT, "malloc_profile_set_trace_cpu");
	if (malloc_profile_set_trace_cpu) {
		malloc_profile_set_trace_cpu(size);
	}
}

void malloc_profile_reset_wrapper()
{
    void (*malloc_profile_reset)() = dlsym(RTLD_DEFAULT, "malloc_profile_reset");
    if (malloc_profile_reset) {
        malloc_profile_reset();
    }
}

void malloc_profile_pause_wrapper()
{
    void (*malloc_profile_pause)() = dlsym(RTLD_DEFAULT, "malloc_profile_pause");
    if (malloc_profile_pause) {
        malloc_profile_pause();
    }
}

void malloc_profile_resume_wrapper()
{
    void (*malloc_profile_resume)() = dlsym(RTLD_DEFAULT, "malloc_profile_resume");
    if (malloc_profile_resume) {
        malloc_profile_resume();
    }
}

void malloc_profile_print_stats_wrapper()
{
    void (*malloc_profile_print_stats)() = dlsym(RTLD_DEFAULT, "malloc_profile_print_stats");
    if (malloc_profile_print_stats) {
        malloc_profile_print_stats();
    }
}
