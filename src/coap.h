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

typedef bool (*PacketFilter)(const uint8_t* data, int len, uint16_t matchMsgId);
class Coap {
  private:
    UDP *_udp;
    int _port;
    
    int bufferSize;
    int maxTokenLen;
    uint8_t *transmittedMessage = NULL;
    CoapMessage coapMessage;
    void insertArrayToBuffer(uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen);
    uint16_t setBuffer();
    static bool isAckMessage(const uint8_t* data, int len, uint16_t matchMsgId);
    void transmitUdpPacket(CoapMessage &msg, uint16_t bufferLen, const char *ip, int port);

    CoapMessageQueue receivedMessageQueue;
    void parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen);
    CoapUri uri;

  public:
    Coap(UDP &udp, int port = DEFAULT_COAP_PORT, int bufferSize = DEFAULT_BUFFER_SIZE);
    ~Coap();
    void beginUdp();
    void initMessage(uint8_t maxTokenLen = 8);

    void setMessage();
    void transmitMessage(const char *ip, int port = DEFAULT_COAP_PORT);

    bool receiveMessage(PacketFilter filter = nullptr, uint16_t matchMsgId = 0);
    void parseReceived(CoapMessage &msg);
    void handleBulkMessage();
};

#endif
