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

#ifndef QBINARYHEAP_H
#define QBINARYHEAP_H

#include <QtCore>

#ifndef Q_ASSERT_FORCE
// Qt's Q_ASSERT is removed in release mode, so use this instead
#define Q_ASSERT_FORCE(cond) ((!(cond)) ? qt_assert(#cond,__FILE__,__LINE__) : qt_noop())
#endif

// keys have type T, priorities have type U
template<typename T, typename U>
class QBinaryHeap
{
public:
	QBinaryHeap(int reserveSize = 128, bool indexed = true) :
		indexed(indexed) {
		reserveSize = nextPow2(reserveSize);
		items.reserve(reserveSize);
		if (indexed) {
			index.reserve(reserveSize);
		}
	}

	bool isEmpty() {
		return items.isEmpty();
	}

	QPair<T, U> findMin() {
		Q_ASSERT_FORCE(!isEmpty());
		return items.first();
	}

	QPair<T, U> takeMin() {
		QPair<T, U> result = findMin();
		if (items.count() == 1) {
			items.clear();
			index.clear();
			return result;
		}
		// 1. replace root with last
		items[0] = items[items.count() - 1];
		items.remove(items.count() - 1);
		setPos(0);

		// 2. bubble down
		int i = 0;
		while (1) {
			U prioP = items[i].second;
			bool hasC1 = leftChild(i) < items.count();
			bool hasC2 = rightChild(i) < items.count();
			if (hasC1 && hasC2) {
				U prioC1 = items[leftChild(i)].second;
				U prioC2 = items[rightChild(i)].second;
				if (prioP < prioC1 && prioP < prioC2)
					break;
				// pick min child and swap
				if (prioC1 < prioC2) {
					qSwap(items[i], items[leftChild(i)]);
					setPos(i);
					i = leftChild(i);
				} else {
					qSwap(items[i], items[rightChild(i)]);
					setPos(i);
					i = rightChild(i);
				}
			} else if (hasC1) {
				U prioC1 = items[leftChild(i)].second;
				if (prioP < prioC1)
					break;
				// swap
				qSwap(items[i], items[leftChild(i)]);
				setPos(i);
				i = leftChild(i);
			} else {
				break;
			}
		}
		setPos(i);

		return result;
	}

	// does not check for duplicates
	void insert(T data, U priority) {
		// 1. Add to end of heap
		items.append(QPair<T,U>(data, priority));
		int i = items.count() - 1;
		repair(i);
	}

	void update(T data, U priority) {
		Q_ASSERT_FORCE(!isEmpty());
		int i = getPos(data);
		items[i].second = priority;
		repair(i);
	}

	U peek(T data) {
		Q_ASSERT_FORCE(!isEmpty());
		int i = getPos(data);
		return items[i].second;
	}

private:
	bool indexed;
	QVector<QPair<T, U> > items;
	QHash<T, int> index;

	void repair(int i) {
		while (i > 0) {
			// 2. Compare with parent and swap if needed, else stop
			int p = parent(i);
			if (items[p].second < items[i].second)
				break;
			qSwap(items[p], items[i]);
			setPos(i);
			i = p;
		}
		setPos(i);
	}

	int getPos(T key) {
		if (indexed) {
			Q_ASSERT_FORCE(index.contains(key));
			return index[key];
		} else {
			for (int i = 0; i < items.count(); i++) {
				if (items[i].first == key)
					return i;
			}
			Q_ASSERT_FORCE(false);
		}
		// never reached
		return -1;
	}

	void setPos(T key, int pos) {
		if (indexed) {
			index[key] = pos;
		}
	}

	void setPos(int pos) {
		setPos(items[pos].first, pos);
	}

	static inline int leftChild(int i) {
		return 2*i + 1;
	}

	static inline int rightChild(int i) {
		return 2*i + 2;
	}

	static inline int parent(int i) {
		return (i-1)/2;
	}

	static int nextPow2(int n) {
		n = n - 1;
		n = n | (n >> 1);
		n = n | (n >> 2);
		n = n | (n >> 4);
		n = n | (n >> 8);
		n = n | (n >> 16);
		n = n + 1;
		return n;
	}
};

void QBinaryHeap_Test();
void QBinaryHeap_TestAll();

#endif // QBINARYHEAP_H
