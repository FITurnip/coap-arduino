#ifndef __COAP_QUEUE_H__
#define __COAP_QUEUE_H__
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

class CoapMessageQueue {
  private:
    uint8_t **queue;
    int *elementLengths;

    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint8_t queueSize;

  public:
    CoapMessageQueue(uint8_t qSize);
    ~CoapMessageQueue();

    bool isEmpty();
    bool isFull();
    bool enqueue(uint8_t *data, int len);
    bool dequeue(uint8_t *data, int &len);
};

#endif
