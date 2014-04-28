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

#include <stdio.h>

#include "pconsumer.h"
#include <QtCore>
#include "qpairingheap.h"
// This is defined in the .pro file
// #define USE_TC_MALLOC
#ifdef USE_TC_MALLOC
#include "google/malloc_extension.h"
#endif

void warmMallocCache()
{
}

int main(int argc, char *argv[])
{
#ifdef USE_TC_MALLOC
	// Don't release memory to the OS
	// MallocExtension::instance()->SetMemoryReleaseRate(0);
	// Don't release memory from the thread unless it has collected more than this size of free areas
	// Default: 16 MB, let's make it 128 MB
	// MallocExtension::instance()->SetNumericProperty("tcmalloc.max_total_thread_cache_bytes", 1ULL << 27);
#endif

	printf("===== Log ===============\n");
	unsigned int seed = clock() ^ time(NULL) ^ getpid();
	qDebug() << "seed:" << seed;
	srand(seed);
    simulationStartTime = get_current_time();
	tsFirstSentPacket = 0;
	runPacketFilter(argc, argv);

#ifdef USE_TC_MALLOC
	printf("===== TCMalloc stats ===============\n");
	size_t value;
	if (MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes", &value)) {
		fprintf(stdout, "Current allocated bytes: %s\n", withCommas(quint64(value)));
	}
	if (MallocExtension::instance()->GetNumericProperty("generic.heap_size", &value)) {
		fprintf(stdout, "Current reserved bytes: %s\n", withCommas(quint64(value)));
	}
	const int kStatsBufferSize = 16 << 10;
	char stats_buffer[kStatsBufferSize] = { 0 };
	MallocExtension::instance()->GetStats(stats_buffer, kStatsBufferSize);
	fprintf(stdout, "%s\n", stats_buffer);
#endif

//	__attribute__((aligned(8))) quint64 x = 6ULL;
//	qDebug() << bit_scan_forward_asm64(x);

    return 0;
}

