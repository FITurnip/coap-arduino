#include "coap_queue.h"
#include <Arduino.h>

CoapMessageQueue::CoapMessageQueue(uint8_t qSize) {
    queueSize = qSize;
    head = 0;
    tail = 0;
    count = 0;

    queue = (uint8_t**) malloc(queueSize * sizeof(uint8_t*));
    elementLengths = (int*) malloc(queueSize * sizeof(uint8_t));

    for (uint8_t i = 0; i < queueSize; i++) {
        queue[i] = nullptr;
        elementLengths[i] = 0;
    }
}

CoapMessageQueue::~CoapMessageQueue() {
    for (uint8_t i = 0; i < queueSize; i++) {
        free(queue[i]);
    }
    free(queue);
    free(elementLengths);
}

// Cek kosong/penuh
bool CoapMessageQueue::isEmpty() { return count == 0; }
bool CoapMessageQueue::isFull()  { return count == queueSize; }

// Enqueue dengan ukuran fleksibel
bool CoapMessageQueue::enqueue(uint8_t *data, int len) {
    if (isFull()) return false;

    if (queue[head] != nullptr) free(queue[head]);

    queue[head] = (uint8_t*) malloc(len * sizeof(uint8_t));
    memcpy(queue[head], data, len);
    elementLengths[head] = len;

    head = (head + 1) % queueSize;
    count++;
    return true;
}

// Dequeue
bool CoapMessageQueue::dequeue(uint8_t *data, int &len) {
    if (isEmpty()) return false;

    len = elementLengths[tail];
    memcpy(data, queue[tail], len);
    free(queue[tail]);
    queue[tail] = nullptr;
    elementLengths[tail] = 0;

    tail = (tail + 1) % queueSize;
    count--;
    return true;
}
