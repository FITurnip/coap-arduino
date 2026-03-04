#ifndef __COAP_H__
#define __COAP_H__
#include "Udp.h"
#include <Arduino.h>
#include "coap_queue.h"

#define DEFAULT_COAP_PORT 5683
#define DEFAULT_BUFFER_SIZE 256
#define COAP_MSG_QUEUE_SIZE 8
#define MAX_URI           4

#define ACK_TIMEOUT       2000
#define ACK_RANDOM_FACTOR 1.5
#define MAX_RETRANSMIT    4
#define NSTART            1
#define DEFAULT_LEISURE   5000
#define PROBING_RATE      1

#define MAX_TRANSMIT_SPAN 45000
#define MAX_TRANSMIT_WAIT 93000
#define MAX_LATENCY       100000
#define PROCESSING_DELAY  2000
#define MAX_RTT           202000
#define EXCHANGE_LIFETIME 247000
#define NON_LIFETIME      145000

typedef struct {
  uint8_t  coapVersion;
  uint8_t  type;
  uint8_t  tokenLen;
  uint8_t  code;
  uint16_t messageId;
  uint8_t token[8];
  uint8_t *options;
  uint8_t *payload;
  uint8_t  payloadLen;
} CoapMessage;

class CoapUri {
  private:
    String uri[MAX_URI];
    void (*handler[MAX_URI])(CoapMessage &msg);
    int counter = 0;
  public:
    void add(String uri, void (*callback)(CoapMessage &msg));
    void handle(String uri, CoapMessage &msg);
};

class CoapBase {
  protected:
    UDP *_udp;
    int _port;
    uint8_t *buffer = NULL; 
    int _maxBufferSize;
    
  public:
    CoapBase(UDP &udp, int port = DEFAULT_COAP_PORT, int maxBufferSize = DEFAULT_BUFFER_SIZE);
    void beginConnection();
};

class CoapTx: public CoapBase {
  private:
    CoapMessage message;
    uint16_t setBuffer();
    void transmitPacket(const char *ip, int port, uint16_t actualBufferSize);
    void insertArrayToBuffer(uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen);

  public:
    using CoapBase::CoapBase;
    void initMessage(uint8_t tokenLen);
    void setMessage();
    void transmitMessage(const char *ip, int port = DEFAULT_COAP_PORT);
};

class CoapRx: public CoapBase {
  private:
    CoapMessageQueue messageQueue;
    void parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen);
    CoapUri uri;

  public:
    CoapBase(UDP &udp, int port = DEFAULT_COAP_PORT, int maxBufferSize = DEFAULT_BUFFER_SIZE);
    bool receiveMessage();
    void parseReceived(CoapMessage &msg);
    void handleBulkMessage();
};
#endif
