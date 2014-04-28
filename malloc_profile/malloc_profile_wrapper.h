/*
 *	Copyright (C) 2011 Ovidiu Mara
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

#ifndef MALLOC_PROFILE_WRAPPER_H
#define MALLOC_PROFILE_WRAPPER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

void malloc_profile_reset_wrapper();
void malloc_profile_pause_wrapper();
void malloc_profile_resume_wrapper();
void malloc_profile_set_trace_cpu_wrapper(size_t size);
void malloc_profile_print_stats_wrapper();
long long malloc_profile_get_alloc_count_wrapper();
long long malloc_profile_get_alloc_count_cpu_wrapper();
long long malloc_profile_get_alloc_count_by_size_wrapper(size_t size);
long long malloc_profile_get_alloc_count_cpu_by_size_wrapper(size_t size);

#ifdef __cplusplus
}
#endif

#endif // MALLOC_PROFILE_WRAPPER_H
