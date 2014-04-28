/*
 *	Copyright (C) 2014 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 is the only version of this
 *  license which this program may be distributed under.
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

#ifndef WAITFREEQUEUEFOLLY_H
#define WAITFREEQUEUEFOLLY_H

#include "../util/producerconsumerqueue.h"

#include <QtCore>

#include "util.h"
#include "ovector.h"

template<typename T>
class WaitFreeQueueFolly {
public:
	WaitFreeQueueFolly() {
		queue = nullptr;
	}

	~WaitFreeQueueFolly() {
		if (queue) {
			delete queue;
			queue = nullptr;
		}
	}

	void init(int maxSize = 100000) {
		Q_ASSERT_FORCE(queue == nullptr);
		queue = new folly::ProducerConsumerQueue<T>(maxSize);
	}

	void enqueue(T item) {
		if (!queue) {
			init();
		}
		queue->write(item);
	}

	void enqueue(const OVector<T> &values) {
		if (!queue) {
			init();
		}
		for (int i = 0; i < values.count(); i++) {
			enqueue(values[i]);
		}
	}

	bool tryDequeue(T &result) {
		if (!queue) {
			init();
		}
		return queue->read(result);
	}

	void dequeueAll(OVector<T> &result) {
		result.clear();
		for (T item; tryDequeue(item); result.append(item)) {
			// Nothing to do
		}
	}

private:
	folly::ProducerConsumerQueue<T> *queue;
};

#endif // WAITFREEQUEUEFOLLY_H
