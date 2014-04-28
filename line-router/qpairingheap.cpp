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

#include "qpairingheap.h"

void QPairingHeap_test()
{
	QPairingHeap<int,quint64> heap;

	const int opCount = 1000000;

	QList<quint64> heapSim;

	for (int i = 0; i < opCount; i++) {
		// insert or delete?
		if (rand() % 2) {
			// insert
			quint64 x = rand();
			heap.insert(0, x);
			heapSim << x;
		} else {
			//delete
			Q_ASSERT(heap.isEmpty() == heapSim.isEmpty());
			if (!heap.isEmpty()) {
				quint64 x1 = heap.findMin().second;
				heap.takeMin();
				quint64 x2 = heapSim.first();
				foreach (quint64 x, heapSim) {
					if (x < x2)
						x2 = x;
				}
				for (int pos = 0; pos < heapSim.count(); pos++) {
					if (heapSim[pos] == x2) {
						heapSim.removeAt(pos);
						break;
					}
				}
				Q_ASSERT_FORCE(x1 == x2);
			}
		}
	}
	//delete
	while (!heap.isEmpty()) {
		Q_ASSERT(heap.isEmpty() == heapSim.isEmpty());
		quint64 x1 = heap.findMin().second;
		heap.takeMin();
		quint64 x2 = heapSim.first();
		foreach (quint64 x, heapSim) {
			if (x < x2)
				x2 = x;
		}
		for (int pos = 0; pos < heapSim.count(); pos++) {
			if (heapSim[pos] == x2) {
				heapSim.removeAt(pos);
				break;
			}
		}
		Q_ASSERT_FORCE(x1 == x2);
	}
	Q_ASSERT(heap.isEmpty() && heapSim.isEmpty());
}

void QPairingHeap_testPerf()
{
	QPairingHeap<int,quint64> heap;

	const int opCount = 1 * 1000 * 1000;

	for (int i = 0; i < opCount; i++) {
		// insert or delete?
		if (rand() % 2) {
			// insert
			quint64 x = rand();
			heap.insert(0, x);
		} else {
			//delete
			if (!heap.isEmpty()) {
				heap.findMin().second;
				heap.takeMin();
			}
		}
	}
	//delete
	while (!heap.isEmpty()) {
		heap.findMin().second;
		heap.takeMin();
	}
	Q_ASSERT(heap.isEmpty());
}
