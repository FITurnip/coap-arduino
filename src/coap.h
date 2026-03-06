#ifndef __COAP_H__
#define __COAP_H__
#include "Udp.h"
#include <Arduino.h>
#include "../lib/CircularQueue/circular_queue.h"

#define DEFAULT_COAP_PORT 5683
#define DEFAULT_BUFFER_SIZE 1152

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

class CoapMessage {
  private:
    enum CoapOptNum : uint16_t {
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

    const char* getCoapCodeName(uint8_t cls, uint8_t detail);
    void printOption(const CoapOpt &opt);

  public:
    uint8_t  coapVersion;
    uint8_t  type;
    uint8_t  tokenLen;
    uint8_t  code;
    uint16_t messageId;
    uint8_t token[8];
    CoapOpt options[32];
    uint8_t optionSize = 0;
    uint8_t *payload;
    uint8_t  payloadLen;
   
    void addOption(uint16_t num, uint16_t len, uint8_t *val);
    void diagnostic();
};

enum CoapMethod : uint8_t {
    COAP_GET    = 1,
    COAP_POST   = 2,
    COAP_PUT    = 3,
    COAP_DELETE = 4
};

typedef void (*CoapHandler)(CoapMessage &request, CoapMessage &response);
class CoapResource {
public:
    CoapResource(const char* p = nullptr);
    void addHandler(const char* path, uint8_t method, CoapHandler handler);
    void handleRequest(const char* path, uint8_t method, CoapMessage& req, CoapMessage& res);

private:
    char* path;
    CoapHandler handlers[5];
    CoapResource* children[10];
    uint8_t childCount;

    CoapResource* findOrCreateChild(const char* name);
    CoapResource* findChild(const char* name);
};

class CoapBase {
  protected:
    UDP *_udp;
    int _port;

    typedef struct {
      uint8_t data[DEFAULT_BUFFER_SIZE];
      uint16_t size;
    } CoapPacket;
    
  public:
    CoapBase(UDP &udp, int port = DEFAULT_COAP_PORT);
    void beginConnection();
};

class CoapTx: public CoapBase {
  private:
    CoapMessage message;
    char* dstIp = NULL;
    int dstPort;

    uint16_t setBuffer(uint8_t *buffer);
    void transmitPacket(const char *ip, int port, uint8_t *buffer, uint16_t actualBufferSize);
    void insertArrayToBuffer(uint8_t *buffer, uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen);
    uint8_t encodeOptionField(uint16_t value, uint8_t out[3]);
    void setDstAddress(const char* ip, int port);
    void decomposeUrlIntoOptions(const char* destUri);
    void normalizeUriPath(char* path);
  public:
    using CoapBase::CoapBase;
    void init(const char *ip, int port, uint8_t tokenLen);
    void setMessage(const char *uri);

    void transmitMessage();
};

class CoapRx: public CoapBase {
  private:
    CircularQueue<CoapPacket, 10> bufferQueue;
    void parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen);
    CoapResource resource;

  public:
    CoapRx(UDP &udp, int port = DEFAULT_COAP_PORT);
    bool receiveMessage();
    void parseReceived(CoapMessage &msg);
    void handleBulkMessage();

    void addHandler(const char* path, uint8_t method, CoapHandler handler);
};

class Coap {
  private:
    CoapTx tx;
    CoapRx rx;
  public:
    Coap();
    void transmmit();
    void receive();
    void handleReceivePacket();
};
#endif
