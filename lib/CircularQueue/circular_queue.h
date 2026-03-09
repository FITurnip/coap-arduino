#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <stddef.h>

template<typename T, size_t Capacity>
class CircularQueue {
private:
    T buffer[Capacity];
    size_t head;
    size_t tail;
    size_t count;

public:
    CircularQueue();

    bool push(const T& item);
    bool pop();

    T& front();
    const T& front() const;

    bool empty() const;
    bool full() const;

    size_t size() const;
    size_t capacity() const;

    void clear();
};

#include "circular_queue.tpp"
#endif
