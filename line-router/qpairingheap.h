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

#ifndef QPAIRINGHEAP_H
#define QPAIRINGHEAP_H

#include <QtCore>

#ifndef Q_ASSERT_FORCE
// Qt's Q_ASSERT is removed in release mode, so use this instead
#define Q_ASSERT_FORCE(cond) ((!(cond)) ? qt_assert(#cond,__FILE__,__LINE__) : qt_noop())
#endif

template<typename T, typename U>
class QPairingHeap;

template<typename T, typename U>
class QPairingHeapPrivate
{
private:
	U priority;
	T data;
	bool empty;
	QList<QPairingHeapPrivate<T,U> *> subheaps;

	QPairingHeapPrivate() :
		priority(0), empty(true) {
		subheaps = QList<QPairingHeapPrivate<T,U> *>();
	}

	QPairingHeapPrivate(T data, U priority) :
		priority(priority), data(data), empty(false) {
		subheaps = QList<QPairingHeapPrivate<T,U> *>();
	}

	~QPairingHeapPrivate() {}

	static QPairingHeapPrivate<T,U> *createHeap() {
		return new QPairingHeapPrivate<T,U>();
	}

	T getData() {
		return data;
	}

	U getPriority() {
		return priority;
	}

	bool isEmpty() {
		return empty;
	}

	QPairingHeapPrivate *findMin() {
		return this;
	}

	QPairingHeapPrivate *merge(QPairingHeapPrivate *other) {
		if (this->isEmpty()) {
			delete this;
			return other;
		} else if (other->isEmpty()) {
			delete other;
			return this;
		} else if (this->priority < other->priority) {
			subheaps.append(other);
			return this;
		} else {
			other->subheaps.append(this);
			return other;
		}
	}

	QPairingHeapPrivate *insert(T data, U priority) {
		QPairingHeapPrivate *basic = new QPairingHeapPrivate(data, priority);
		return merge(basic);
	}

	QPairingHeapPrivate *deleteMin() {
		if (isEmpty()) {
			return this;
		} else if (subheaps.isEmpty()) {
			this->empty = true;
			return this;
		} else if (subheaps.count() == 1) {
			QPairingHeapPrivate *result = *subheaps.begin();
			subheaps.erase(subheaps.begin());
			delete this;
			return result;
		} else {
			QPairingHeapPrivate *result = mergePairs(subheaps);
			delete this;
			return result;
		}
	}

	// remove the subtree with data==T from the tree, update prio and return it, or return the root
	QPairingHeapPrivate *update(T data, U priority) {
		if (getData() == data) {
			Q_ASSERT_FORCE(priority <= this->priority);
			this->priority = priority;
			return this;
		} else {
			// remove the subtree with data==T from the tree and return it
			for (int i = 0; i < subheaps.count(); i++) {
				QPairingHeapPrivate *n = subheaps[i]->update(data, priority);
				if (n == subheaps[i]) {
					// detach it
					subheaps.removeAt(i);
					return n;
				}
				if (n != NULL) {
					return n;
				}
			}
		}
		// not found
		return NULL;
	}

	QPairingHeapPrivate *mergePairs(QList<QPairingHeapPrivate<T,U> *> list) {
		Q_ASSERT_FORCE(!list.isEmpty());
		if (list.count() == 1)
			return list.first();

		// merge pairs from left to right
		QList<QPairingHeapPrivate*> pairsLR;
		// for (int i = 0; i < list.count() - 1; i += 2) {
		for (typename QList<QPairingHeapPrivate<T,U> *>::iterator i = list.begin();
			 !list.empty() && i != list.end() && i+1 != list.end();
			 ++i, ++i) {
			// pairsLR.append(list[i]->merge(list[i+1]));
			pairsLR.append((*i)->merge(*(i+1)));
		}
		if (list.count() % 2)
			pairsLR.append(list.last());

		// merge cumulatively from right to left
		while (pairsLR.count() >= 2) {
			QPairingHeapPrivate* r2 = *(pairsLR.end() - 1);
			pairsLR.erase(pairsLR.end() - 1);
			QPairingHeapPrivate* r1 = *(pairsLR.end() - 1);
			pairsLR.erase(pairsLR.end() - 1);
			pairsLR.append(r1->merge(r2));
		}
		return pairsLR.first();
	}

	U peek(T data, bool &found) {
		found = false;
		if (getData() == data) {
			found = true;
			return getPriority();
		} else {
			for (int i = 0; i < subheaps.count(); i++) {
				U prio = subheaps[i]->peek(data, found);
				if (found)
					return prio;
			}
		}
		return 0;
	}

	friend class QPairingHeap<T,U>;
};

template<typename T, typename U>
class QPairingHeap {
public:
	QPairingHeap() {
		heap = QPairingHeapPrivate<T,U>::createHeap();
	}

	~QPairingHeap() {
		delete heap;
	}

	bool isEmpty() {
		return heap->isEmpty();
	}

	bool empty() {
		return isEmpty();
	}

	QPair<T, U> findMin() {
		Q_ASSERT_FORCE(!heap->isEmpty());

		return QPair<T, U>(heap->getData(), heap->getPriority());
	}

	QPair<T, U> takeMin() {
		QPair<T, U> result = findMin();
		heap = heap->deleteMin();
		return result;
	}

	// does not check for duplicates
	void insert(T data, U priority) {
		heap = heap->insert(data, priority);
	}

	void merge(QPairingHeap<T,U> *other) {
		heap = heap->merge(other->heap);
		other->heap = heap;
	}

	void update(T data, U priority) {
		Q_ASSERT_FORCE(!heap->isEmpty());
		QPairingHeapPrivate<T,U> *subtree = heap->update(data, priority);
		Q_ASSERT_FORCE(subtree);
		if (subtree == heap) {
			// we're done
			return;
		}
		// merge
		heap = heap->merge(subtree);
	}

	U peek(T data) {
		bool found;
		U prio = heap->peek(data, found);
		Q_ASSERT_FORCE(found);
		return prio;
	}

private:
	QPairingHeapPrivate<T,U> *heap;
};

void QPairingHeap_test();
void QPairingHeap_testPerf();


#endif // QPAIRINGHEAP_H
