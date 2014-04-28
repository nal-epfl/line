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

#include "qbinaryheap.h"

void QBinaryHeap_Test()
{
	QBinaryHeap<int, int> heap;
	QList<QPair<int, int> > heapSim;

	const int opCount = 10000;
	const int maxVal = INT_MAX;

	for (int iOp = 0; iOp < opCount; iOp++) {
		// insert, delete, update
		int op = rand() % 3;
		if (op == 0) {
			// insert
			int x, prio;
			while (1) {
				x = rand() % maxVal;
				prio = rand() % maxVal;
				bool valid = true;
				for (int i = 0; i < heapSim.count(); i++) {
					if (heapSim[i].first == x || heapSim[i].second == prio) {
						valid = false;
						break;
					}
				}
				if (valid)
					break;
			}
			heap.insert(x, prio);
			heapSim << QPair<int, int>(x, prio);
		} else if (op == 1) {
			// update key
			//continue;
			Q_ASSERT(heap.isEmpty() == heapSim.isEmpty());
			if (!heap.isEmpty()) {
				int i = rand() % heapSim.count();
				int x = heapSim[i].first;
				if (heapSim[i].second == 0)
					continue;
				int prio;
				while (1) {
					prio = rand() % maxVal;
					if (prio >= heapSim[i].second)
						continue;
					bool valid = true;
					for (int i = 0; i < heapSim.count(); i++) {
						if (heapSim[i].second == prio) {
							valid = false;
							break;
						}
					}
					if (valid)
						break;
				}
				heap.update(x, prio);
				heapSim[i].second = prio;
			}
		} else {
			//delete
			Q_ASSERT(heap.isEmpty() == heapSim.isEmpty());
			if (!heap.isEmpty()) {
				QPair<int, int> kp1 = heap.takeMin();
				QPair<int, int> kp2 = heapSim.first();
				int i;
				int iMin = 0;
				for (i = 0; i < heapSim.count(); i++) {
					if (heapSim[i].second < kp2.second) {
						kp2 = heapSim[i];
						iMin = i;
					}
				}
				heapSim.removeAt(iMin);
				Q_ASSERT_FORCE(kp1 == kp2);
			}
		}
	}
	//delete
	while (!heap.isEmpty()) {
		Q_ASSERT(heap.isEmpty() == heapSim.isEmpty());
		QPair<int, int> kp1 = heap.takeMin();
		QPair<int, int> kp2 = heapSim.first();
		int i;
		int iMin = 0;
		for (i = 0; i < heapSim.count(); i++) {
			if (heapSim[i].second < kp2.second) {
				kp2 = heapSim[i];
				iMin = i;
			}
		}
		heapSim.removeAt(iMin);
		Q_ASSERT_FORCE(kp1 == kp2);
	}
	Q_ASSERT_FORCE(heap.isEmpty() && heapSim.isEmpty());
}

void QBinaryHeap_TestAll()
{
	for (int i = 0; i < 10000; i++) {
		QBinaryHeap_Test();
		qDebug() << "run OK" << i;
	}
}
