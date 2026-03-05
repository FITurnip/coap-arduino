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

enum class CoapOptNum : uint16_t {
    IF_MATCH       = 1,
    URI_HOST       = 3,
    ETAG           = 4,
    IF_NONE_MATCH  = 5,
    OBSERVE        = 6,    // RFC 7641
    URI_PORT       = 7,
    LOCATION_PATH  = 8,
    URI_PATH       = 11,
    CONTENT_FORMAT = 12,
    MAX_AGE        = 14,
    URI_QUERY      = 15,
    ACCEPT         = 17,
    LOCATION_QUERY = 20,
    BLOCK2         = 23,   // RFC 7959
    BLOCK1         = 27,   // RFC 7959
    SIZE2          = 28,   // RFC 7959
    PROXY_URI      = 35,
    PROXY_SCHEME   = 39,
    SIZE1          = 60
};

typedef struct {
    uint16_t num;
    uint16_t len;
    uint8_t *val;
} CoapOpt;

class CoapMessage {
  private:
    const char* getCoapCodeName(uint8_t cls, uint8_t detail);

  public:
    uint8_t  coapVersion;
    uint8_t  type;
    uint8_t  tokenLen;
    uint8_t  code;
    uint16_t messageId;
    uint8_t token[8];
    CoapOpt options[8];
    uint8_t optionSize = 0;
    uint8_t *payload;
    uint8_t  payloadLen;
   
    void addOption(uint16_t num, uint16_t len, uint8_t *val);
    void diagnostic();
};

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
    uint8_t encodeOptionField(uint16_t value, uint8_t out[3]);
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
    CoapRx(UDP &udp, int port = DEFAULT_COAP_PORT, int maxBufferSize = DEFAULT_BUFFER_SIZE);

    bool receiveMessage();
    void parseReceived(CoapMessage &msg);
    void handleBulkMessage();
};
#endif
