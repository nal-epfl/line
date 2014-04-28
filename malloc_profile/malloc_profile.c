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

// gcc -O2 -Wall -fPIC -std=gnu99 -o malloc_profile.so -shared malloc_profile.c -ldl -lrt
// LD_PRELOAD="$(pwd)/malloc_profile.so" whoami

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <bits/time.h>
#include <time.h>
#include <sched.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

typedef long long qint64;
typedef long long unsigned quint64;
typedef u_int32_t quint32;

void malloc_profile_reset();
void malloc_profile_pause();
void malloc_profile_resume();
void malloc_profile_print_stats();
void malloc_profile_set_trace_cpu(size_t size);
qint64 malloc_profile_get_alloc_count();
qint64 malloc_profile_get_alloc_count_cpu();
qint64 malloc_profile_get_alloc_count_by_size(size_t size);
qint64 malloc_profile_get_alloc_count_cpu_by_size();

static void __attribute__ ((constructor)) malloc_profile_load();
static void __attribute__ ((destructor)) malloc_profile_destroy();

static void *(*calloc_original)(size_t count, size_t size) = NULL;
static void *(*realloc_original)(void *p, size_t size) = NULL;
static void *(*malloc_original)(size_t size) = NULL;
static void *(*free_original)(void *p) = NULL;
static void *(*exit_original)(int status) __attribute__ ((__noreturn__)) = NULL;

static volatile int paused = 0;

#define SEC_TO_NSEC  1000000000ULL
#define MSEC_TO_NSEC 1000000ULL
#define USEC_TO_NSEC 1000ULL

// essentialy the integer base 2 logarithm
static inline quint64 bit_scan_reverse_asm64(quint64 v)
{
	if (v == 0)
		return 0;
	quint64 r;
	asm volatile("bsrq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

static quint64 get_current_time()
{
	struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
	return ((quint64)ts.tv_sec) * SEC_TO_NSEC + ((quint64)ts.tv_nsec);
}

static void with_commas_helper(quint64 n, char *s) {
	if (n < 1000ULL) {
		char buffer[50];
		*buffer = '\0';
		sprintf(buffer, "%llu", n);
		strcat(s, buffer);
	} else {
		with_commas_helper(n/1000ULL, s);
		char buffer[50];
		*buffer = '\0';
		sprintf(buffer, ",%03llu", n % 1000ULL);
		strcat(s, buffer);
	}
}

static void with_commas(quint64 n, char *s) {
	*s = '\0';
	with_commas_helper(n, s);
}

static void nice_power_of_2_to_str(quint64 logarithm, char *str)
{
	quint64 value = 1ULL << logarithm;
	if (logarithm < 10) {
		sprintf(str, "%llu  ", value);
	} else if (logarithm < 20) {
		sprintf(str, "%llu k", value >> 10);
	} else if (logarithm < 30) {
		sprintf(str, "%llu M", value >> 20);
	} else if (logarithm < 40) {
		sprintf(str, "%llu G", value >> 30);
	} else if (logarithm < 50) {
		sprintf(str, "%llu T", value >> 40);
	} else if (logarithm < 60) {
		sprintf(str, "%llu P", value >> 50);
	} else {
		sprintf(str, "%llu E", value >> 60);
	}
}

static void nice_time_to_str(quint64 nanoseconds, char *str)
{
	quint64 microseconds = 0;
	quint64 ms = 0;
	quint64 seconds = 0;
	quint64 minutes = 0;
	quint64 hours = 0;
	quint64 days = 0;

	if (nanoseconds >= 1000ULL) {
		microseconds = nanoseconds / 1000ULL;
		nanoseconds = nanoseconds % 1000ULL;
	}

	if (microseconds >= 1000ULL) {
		ms = microseconds / 1000ULL;
		microseconds = microseconds % 1000ULL;
	}

	if (ms >= 1000ULL) {
		seconds = ms / 1000ULL;
		ms = ms % 1000ULL;
	}

	if (seconds >= 60ULL) {
		minutes = seconds / 60ULL;
		seconds = seconds % 60ULL;
	}

	if (minutes >= 60ULL) {
		hours = minutes / 60ULL;
		minutes = minutes % 60ULL;
	}

	if (hours >= 24ULL) {
		days = hours / 24ULL;
		hours = hours % 24ULL;
	}

	if (days > 0) {
		sprintf(str, "%llu d %llu h %llu m %llu s %llu ms %llu us %llu ns",
				days, hours, minutes, seconds, ms, microseconds, nanoseconds);
	} else if (hours > 0) {
		sprintf(str, "%llu h %llu m %llu s %llu ms %llu us %llu ns",
				hours, minutes, seconds, ms, microseconds, nanoseconds);
	} else if (minutes > 0) {
		sprintf(str, "%llu m %llu s %llu ms %llu us %llu ns",
				minutes, seconds, ms, microseconds, nanoseconds);
	} else if (seconds > 0) {
		sprintf(str, "%llu s %llu ms %llu us %llu ns",
				seconds, ms, microseconds, nanoseconds);
	} else if (ms > 0) {
		sprintf(str, "%llu ms %llu us %llu ns",
				ms, microseconds, nanoseconds);
	} else if (microseconds > 0) {
		sprintf(str, "%llu us %llu ns",
				microseconds, nanoseconds);
	} else {
		sprintf(str, "%llu ns",
				nanoseconds);
	}
}

#define kMaxCPUs 4

static int get_cpu()
{
	return sched_getcpu() % kMaxCPUs;
}

static int get_cpu_count()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

#define kNumSizeCounters 32
static quint32 size_counters[kMaxCPUs][kNumSizeCounters];

#define kNumDelayCounters 16
static quint32 delay_counters[kMaxCPUs][kNumDelayCounters];

static quint64 trace_masks[kMaxCPUs];

static void show_backtrace() {
	unw_cursor_t cursor;
	unw_context_t context;
	unw_getcontext(&context);
	unw_init_local(&cursor, &context);

	fprintf(stderr, "Backtrace:\n");
	while (unw_step(&cursor) > 0) {
		unw_word_t offset;
		char fname[128];
		fname[0] = '\0';
		(void) unw_get_proc_name(&cursor, fname, sizeof(fname), &offset);
		fprintf(stderr, "%s\n", fname);
	}
}

static void record_size(int cpu, quint64 size)
{
	if (trace_masks[cpu] && size >= trace_masks[cpu]) {
		int old_paused = paused;
		paused = 1;
		show_backtrace();
		paused = old_paused;
	}
	quint64 logarithm = bit_scan_reverse_asm64(size);
	if (logarithm >= kNumSizeCounters) {
		logarithm = kNumSizeCounters - 1;
	}
	__sync_add_and_fetch(&size_counters[cpu][logarithm], 1);
}

static void record_delay(int cpu, quint64 delta)
{
	quint64 logarithm = bit_scan_reverse_asm64(delta);
	if (logarithm >= kNumDelayCounters) {
		logarithm = kNumDelayCounters - 1;
	}
	__sync_add_and_fetch(&delay_counters[cpu][logarithm], 1);
}

void malloc_profile_print_stats()
{
	__sync_synchronize();
	fprintf(stderr, "Memory allocation profiling\n");
	for (int cpu = 0; cpu < get_cpu_count() && cpu < kMaxCPUs; cpu++) {
		fprintf(stderr, "CPU %2d: Requested size counters:\n", cpu);
		for (int i = 0; i < kNumSizeCounters; i++) {
			if (size_counters[cpu][i] == 0)
				continue;
			char buffer1[50];
			char buffer2[50];
			char buffer3[50];
			with_commas(size_counters[cpu][i], buffer3);
			nice_power_of_2_to_str(i, buffer1);
			nice_power_of_2_to_str(i + 1, buffer2);
			fprintf(stderr, "%15sB  to  %15sB:  %15s\n",
					buffer1, buffer2, buffer3);
		}
		fprintf(stderr, "CPU %2d: Delay counters:\n", cpu);
		for (int i = 0; i < kNumDelayCounters; i++) {
			if (delay_counters[cpu][i] == 0)
				continue;
			char buffer1[50];
			char buffer2[50];
			char buffer3[50];
			with_commas(delay_counters[cpu][i], buffer3);
			nice_time_to_str(1ULL << i, buffer1);
			nice_time_to_str(1ULL << (i + 1), buffer2);
			fprintf(stderr, "%16s  to  %16s:  %15s\n",
					buffer1, buffer2, buffer3);
		}
		fprintf(stderr, "\n");
		fflush(stderr);
	}
}

void malloc_profile_set_trace_cpu(size_t size)
{
	trace_masks[get_cpu()] = size;
}

static void malloc_profile_load()
{
	malloc_profile_reset();
	realloc_original = dlsym(RTLD_NEXT, "realloc");
	malloc_original = dlsym(RTLD_NEXT, "malloc");
	calloc_original = dlsym(RTLD_NEXT, "calloc");
	free_original = dlsym(RTLD_NEXT, "free");
	exit_original = dlsym(RTLD_NEXT, "exit");
}

void malloc_profile_reset()
{
	for (int cpu = 0; cpu < kMaxCPUs; cpu++) {
		for (int i = 0; i < kNumSizeCounters; i++) {
			size_counters[cpu][i] = 0;
		}
		for (int i = 0; i < kNumDelayCounters; i++) {
			delay_counters[cpu][i] = 0;
		}
		trace_masks[cpu] = 0;
	}
	__sync_synchronize();
}

void malloc_profile_pause()
{
	paused = 1;
	__sync_synchronize();
}

void malloc_profile_resume()
{
	paused = 0;
	__sync_synchronize();
}

qint64 malloc_profile_get_alloc_count()
{
	qint64 result = 0;
	for (int cpu = 0; cpu < kMaxCPUs; cpu++) {
		for (int i = 0; i < kNumSizeCounters; i++) {
			result += size_counters[cpu][i];
		}
	}
	return result;
}

qint64 malloc_profile_get_alloc_count_by_size(size_t size)
{
	quint64 logarithm = bit_scan_reverse_asm64(size);
	if (logarithm >= kNumSizeCounters) {
		logarithm = kNumSizeCounters - 1;
	}
	qint64 result = 0;
	for (int cpu = 0; cpu < kMaxCPUs; cpu++) {
		result += size_counters[cpu][logarithm];
	}
	return result;
}

qint64 malloc_profile_get_alloc_count_cpu()
{
	qint64 result = 0;
	int cpu = get_cpu();
	for (int i = 0; i < kNumSizeCounters; i++) {
		result += size_counters[cpu][i];
	}
	return result;
}

qint64 malloc_profile_get_alloc_count_cpu_by_size(size_t size)
{
	int cpu = get_cpu();
	quint64 logarithm = bit_scan_reverse_asm64(size);
	if (logarithm >= kNumSizeCounters) {
		logarithm = kNumSizeCounters - 1;
	}
	return size_counters[cpu][logarithm];
}

static void malloc_profile_destroy()
{
	malloc_profile_print_stats();
}

void *realloc(void *p, size_t size)
{
	if (!realloc_original)
		return NULL;
	if (paused) {
		return realloc_original(p, size);
	}
	int cpu = get_cpu();
	record_size(cpu, size);
	quint64 ts1 = get_current_time();
	void *result = realloc_original(p, size);
	quint64 ts2 = get_current_time();
	record_delay(cpu, ts2 - ts1);
	return result;
}

void *calloc(size_t count, size_t size)
{
	if (!calloc_original)
		return NULL;
	if (paused) {
		return calloc_original(count, size);
	}
	int cpu = get_cpu();
	record_size(cpu, count * size);
	quint64 ts1 = get_current_time();
	void *result = calloc_original(count, size);
	quint64 ts2 = get_current_time();
	record_delay(cpu, ts2 - ts1);
	return result;
}

void *malloc(size_t size)
{
	if (!malloc_original)
		return NULL;
	if (paused) {
		return malloc_original(size);
	}
	int cpu = get_cpu();
	record_size(cpu, size);
	quint64 ts1 = get_current_time();
	void *result = malloc_original(size);
	quint64 ts2 = get_current_time();
	record_delay(cpu, ts2 - ts1);
	return result;
}

void free(void *p)
{
	if (!free_original)
		return;
	if (paused) {
		free_original(p);
		return;
	}
	int cpu = get_cpu();
	quint64 ts1 = get_current_time();
	free_original(p);
	quint64 ts2 = get_current_time();
	record_delay(cpu, ts2 - ts1);
}

void exit(int status)
{
	malloc_profile_print_stats();
	if (exit_original) {
		exit_original(status);
	} else {
		abort();
	}
}
