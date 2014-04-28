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

#ifndef SPINLOCKEDQUEUE_H
#define SPINLOCKEDQUEUE_H

#include <pthread.h>
#include <QLinkedList>
#include <QVector>


#define PROFILE_SPINLOCKEDQUEUE 0

#if PROFILE_SPINLOCKEDQUEUE

#define SEC_TO_NSEC  1000000000
#define MSEC_TO_NSEC 1000000
#define USEC_TO_NSEC 1000

#define TS_FORMAT "%llu %3llus %3llum %3lluu %3llun"
#define TS_FORMAT_PARAM(X) (X)/SEC_TO_NSEC/1000, ((X)/SEC_TO_NSEC)%1000, ((X)/MSEC_TO_NSEC)%1000, ((X)/USEC_TO_NSEC)%1000, (X)%1000

#endif

quint64 get_current_time();

// A thread safe queue protected by a spinlock.
template<typename T>
class SpinlockedQueue {
public:
    // Creates a SpinlockedQueue instance.
	SpinlockedQueue() {
		pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
	}

    // Destroys the SpinlockedQueue instance.
	~SpinlockedQueue() {
		pthread_spin_destroy(&spinlock);
	}

	void init(int maxSize = 1000) {
		pthread_spin_lock(&spinlock);
		items.reserve(maxSize);
		pthread_spin_unlock(&spinlock);
	}

	// Enqueues an item. Thread safe.
	void enqueue(T item)
    {
		pthread_spin_lock(&spinlock);
		items.append(item);
		pthread_spin_unlock(&spinlock);
    }

	// Enqueues an item with a waiting time constraint. Thread safe.
	// Returns true if the item was enqueued.
	// Always tries to enqueue at least once, even if the time limit is zero.
	bool enqueue(T item, quint64 maxWaitTimeNs)
	{
		quint64 tExit = 0;
		while (1) {
			int ret = pthread_spin_trylock(&spinlock);
			if (ret == 0) {
				// got the lock
				items.append(item);
				pthread_spin_unlock(&spinlock);
				return true;
			} else {
				if (tExit == 0) {
					get_current_time() + maxWaitTimeNs;
				} else {
					quint64 tNow = get_current_time();
					if (tNow >= tExit) {
						return false;
					}
				}
			}
		}
	}

	// Enqueues a list of items. Thread safe.
	void enqueue(const QVector<T> &values)
	{
		pthread_spin_lock(&spinlock);
		for (int i = 0; i < values.count(); i++) {
			items.append(values[i]);
		}
		pthread_spin_unlock(&spinlock);
	}

	// Enqueues a list of items with a waiting time constraint. Thread safe.
	// Returns true if the items were enqueued.
	// Always tries to enqueue at least once, even if the time limit is zero.
	bool enqueue(const QVector<T> &values, quint64 maxWaitTimeNs)
	{
		quint64 tExit = 0;
		while (1) {
			int ret = pthread_spin_trylock(&spinlock);
			if (ret == 0) {
				// got the lock
				for (int i = 0; i < values.count(); i++) {
					items.append(values[i]);
				}
				pthread_spin_unlock(&spinlock);
				return true;
			} else {
				if (tExit == 0) {
					tExit = get_current_time() + maxWaitTimeNs;
				} else {
					quint64 tNow = get_current_time();
					if (tNow >= tExit) {
						return false;
					}
				}
			}
		}
	}

    // Dequeues the item at the head of the queue, non-blocking (returns false if the list is empty).
	// However it may block until the lock is acquired.
    // Thread safe.
    bool tryDequeue(T &result)
    {
		bool finished = false;
		pthread_spin_lock(&spinlock);
		if (!items.isEmpty()) {
			result = items.first();
			items.remove(0);
			finished = true;
		}
		pthread_spin_unlock(&spinlock);

		return finished;
    }

	// Dequeues the item at the head of the queue, non-blocking, with a waiting time constraint.
	// Returns false if the list is empty or the lock could not be acquired during the time
	// limit.
	bool tryDequeue(T &result, quint64 maxWaitTimeNs)
	{
		quint64 tExit = 0;
		while (1) {
			int ret = pthread_spin_trylock(&spinlock);
			if (ret == 0) {
				// got the lock
				bool finished = false;
				if (!items.isEmpty()) {
					result = items.takeFirst();
					finished = true;
				}
				pthread_spin_unlock(&spinlock);
				return finished;
			} else {
				if (tExit == 0) {
					tExit = get_current_time() + maxWaitTimeNs;
				} else {
					quint64 tNow = get_current_time();
					if (tNow >= tExit) {
						return false;
					}
				}
			}
		}
	}

    // Dequeues and returns all the items in the queue.
	// It may block until the lock is acquired, but does not block if the queue is empty.
    // Thread safe.
    void dequeueAll(QVector<T> &result)
	{
		result.clear();

#if PROFILE_SPINLOCKEDQUEUE
		quint64 ts1 = get_current_time();
#endif

		pthread_spin_lock(&spinlock);
		if (!items.isEmpty()) {
			result = items;
			items.clear();
		}
		pthread_spin_unlock(&spinlock);

#if PROFILE_SPINLOCKEDQUEUE
		quint64 ts2 = get_current_time();
		if (ts2 - ts1 > 100 * USEC_TO_NSEC)
			printf("dequeue: ts delta = + "TS_FORMAT" \n", TS_FORMAT_PARAM(ts2 - ts1));
#endif
	}

	// Dequeues and returns all the items in the queue, with a waiting time constraint.
	// Returns an empty list if the lock could not be acquired within the time limit.
	// Thread safe.
	void dequeueAll(QVector<T> &result, quint64 maxWaitTimeNs)
	{
		result.clear();

		quint64 tExit = 0;
		while (1) {
			int ret = pthread_spin_trylock(&spinlock);
			if (ret == 0) {
				// got the lock
				if (!items.isEmpty()) {
					result = items;
					items.clear();
				}
				pthread_spin_unlock(&spinlock);
				return;
			} else {
				if (tExit == 0) {
					tExit = get_current_time() + maxWaitTimeNs;
				} else {
					quint64 tNow = get_current_time();
					if (tNow >= tExit) {
						return;
					}
				}
			}
		}
	}

private:
	QVector<T> items;
	pthread_spinlock_t spinlock;
};

#endif // SPINLOCKEDQUEUE_H
