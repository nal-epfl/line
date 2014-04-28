#include "waitfreequeue.h"

// usage example
void waitFreeQueueExample()
{
    WaitFreeQueue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    int v;
    bool b = q.tryDequeue(v);
    b = q.tryDequeue(v);
    if (b) {
        qDebug() << v;
    }
    q.enqueue(3);
    q.enqueue(4);
    b = q.tryDequeue(v);
    if (b) {
        qDebug() << v;
    }
    b = q.tryDequeue(v);
    if (b) {
        qDebug() << v;
    }
    b = q.tryDequeue(v);
    if (b) {
        qDebug() << v;
    }
}
