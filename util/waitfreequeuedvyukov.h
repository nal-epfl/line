/*
 *	Copyright (C) 2009 Dmitry Vyukov
 *  Copyright (C) 2014 Ovidiu Mara
 *
 *	http://software.intel.com/en-us/articles/single-producer-single-consumer-queue
 *  http://www.1024cores.net/home/lock-free-algorithms/queues/unbounded-spsc-queue
 *
 *	Redistribution and use in source and binary forms, with or without modification, are
 *  permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DMITRY VYUKOV OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those of the authors
 *  and should not be interpreted as representing official policies, either expressed or implied,
 *  of Dmitry Vyukov.
*/

#ifndef WAITFREEQUEUEDVYUKOV_H
#define WAITFREEQUEUEDVYUKOV_H

#if ( (__GNUC__ == 4) && (__GNUC_MINOR__ >= 1) || __GNUC__ > 4) && (defined(__x86_64__) || defined(__i386__))
#define __memory_barrier __sync_synchronize
#endif

#include <QtCore>

// Cache line size on modern x86 processors (in bytes)
const size_t cacheLineSize = 64;

// Single-producer/single-consumer wait-free queue.
// Warning: some operations might call new, which might wait!
template<typename T>
class WaitFreeQueueDVyukov {
public:
	WaitFreeQueueDVyukov() {
        Node* n = new Node;
        n->next = nullptr;
        tail = head = first = tailCopy = n;
    }

	~WaitFreeQueueDVyukov() {
        Node* n = first;
        do {
            Node* next = n->next;
            delete n;
            n = next;
        } while (n);
    }

	void init(int maxSize = 1000) {
		T dummy = T();
		for (int i = 0; i < maxSize; i++) {
			enqueue(dummy);
		}
		QVector<T> garbage;
		dequeueAll(garbage);
	}

    // Might wait in new()
    void enqueue(T item) {
        Node* n = allocNode();
        n->next = nullptr;
        n->value = item;
        storeRelease(&head->next, n);
        head = n;
    }

    void enqueue(const QVector<T> &items) {
        for (int i = 0; i < items.count(); i++) {
            enqueue(items[i]);
        }
    }

    // Returns false if queue is empty
    bool tryDequeue(T &result) {
		if (loadConsume(&tail->next) != nullptr) {
            result = tail->next->value;
            storeRelease(&tail, tail->next);
            return true;
        } else {
            return false;
        }
    }

    // Might wait in new()
    void dequeueAll(QVector<T> &result) {
        result.clear();
        for (T item; tryDequeue(item); result.append(item)) {
            // Nothing to do
        }
    }

private:
    // Internal node structure
    struct Node {
        Node* next;
        T value;
    };

    // Consumer part
    // Accessed mainly by consumer, infrequently be producer
    Node* tail; // tail of the queue

    // Delimiter between consumer part and producer part,
    // so that they are stored on different cache lines
    char cacheLinePad[cacheLineSize];

    // Producer part
    // Accessed only by producer
    Node* head; // Head of the queue
    Node* first; // Last unused node (tail of node cache)
    Node* tailCopy; // Helper (points somewhere between first and tail)

    Node* allocNode() {
        // First tries to allocate node from internal node cache.
        // If attempt fails, allocates node via ::operator new()
        if (first != tailCopy) {
            Node* n = first;
            first = first->next;
            return n;
        }
        tailCopy = loadConsume(&tail);
        if (first != tailCopy) {
            Node* n = first;
            first = first->next;
            return n;
        }
        Node* n = new Node;
        return n;
    }

	WaitFreeQueueDVyukov(WaitFreeQueueDVyukov const&);
	WaitFreeQueueDVyukov& operator = (WaitFreeQueueDVyukov const&);

    // Load with 'consume' (data-dependent) memory ordering
    template<typename U>
    static U loadConsume(U const* addr)
    {
        // Hardware fence is implicit on x86
        U v = *const_cast<U const volatile*>(addr);
        __memory_barrier(); // compiler fence
        return v;
    }

    // Store with 'release' memory ordering
    template<typename U>
    static void storeRelease(U* addr, U v)
    {
        // Hardware fence is implicit on x86
        __memory_barrier(); // compiler fence
        *const_cast<U volatile*>(addr) = v;
    }
};

void WaitFreeQueueDVyukovExample();

#endif // WAITFREEQUEUEDVYUKOV_H
