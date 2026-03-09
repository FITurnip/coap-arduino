#ifndef __COAP_H__
#define __COAP_H__
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "../lib/CircularQueue/circular_queue.h"
#include "../lib/StaticList/static_list.h"

#define DEFAULT_COAP_PORT 5683
#define DEFAULT_BUFFER_SIZE 1152
#define DEFAULT_PAYLOAD_SIZE 1024
#define MAX_MSG_ENTRIES 10
#define MAX_OPTION_VAL_LEN 64
#define MAX_OPTIONS 32

#define ACK_TIMEOUT       2000
#define ACK_RANDOM_FACTOR 1.5
#define MAX_RETRANSMIT    4
#define NSTART            1       // skipped, for now use it for waiting list just one
#define DEFAULT_LEISURE   5000
#define PROBING_RATE      1

#define MAX_TRANSMIT_SPAN 45000
#define MAX_TRANSMIT_WAIT 93000
#define MAX_LATENCY       100000
#define PROCESSING_DELAY  2000
#define MAX_RTT           202000
#define EXCHANGE_LIFETIME 247000
#define NON_LIFETIME      145000

enum CoapMethod : uint8_t {
  COAP_EMPTY  = 0,
  COAP_GET    = 1,
  COAP_POST   = 2,
  COAP_PUT    = 3,
  COAP_DELETE = 4
};

enum CoapType : uint8_t {
  COAP_CON  = 0,
  COAP_NON  = 1,
  COAP_ACK  = 2,
  COAP_RST  = 3
};

class CoapMessage {
private:
  enum CoapOptNum : uint16_t {
    IF_MATCH       = 1,
    URI_HOST       = 3,
    ETAG           = 4,
    IF_NONE_MATCH  = 5,
    URI_PORT       = 7,
    LOCATION_PATH  = 8,
    URI_PATH       = 11,
    CONTENT_FORMAT = 12,
    MAX_AGE        = 14,
    URI_QUERY      = 15,
    ACCEPT         = 17,
    LOCATION_QUERY = 20,
    PROXY_URI      = 35,
    PROXY_SCHEME   = 39,
    SIZE1          = 60
    };

    typedef struct {
      uint16_t num;
      uint16_t len;
      uint8_t val[MAX_OPTION_VAL_LEN];
    } CoapOpt;
    const char* getCoapCodeName(uint8_t cls, uint8_t detail);
    void printOption(const CoapOpt &opt);

  public:
    uint8_t  coapVersion = 1;
    uint8_t  type;
    uint8_t  tokenLen;
    uint8_t  code;
    uint16_t messageId;
    uint8_t token[8];
    CoapOpt options[MAX_OPTIONS];
    uint8_t optionSize = 0;
    uint8_t payload[DEFAULT_PAYLOAD_SIZE];
    uint16_t  payloadLen = 0;

    char dstIp[40];
    int dstPort;

    void addOption(uint16_t num, uint16_t len, const uint8_t *val);
    void diagnostic();
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

typedef struct {
  const char *ssid, *password;
  IPAddress localIP, gateway, subnet;
} WifiCfg;

class CoapSocket {
  protected:
    WiFiUDP _udp;
    int _port;

    typedef struct {
      uint8_t data[DEFAULT_BUFFER_SIZE];
      uint16_t size;
    } CoapPacket;

    CoapPacket lastPacketReceived;
    
    bool _transmit(const char *ip, int port, CoapPacket &packet);
    bool _receive();
  public:
    CoapSocket(int port = DEFAULT_COAP_PORT);
    void start(WifiCfg wifiCfg);

    WiFiUDP* getTransport();
    void setTransport(WiFiUDP* transport);
};

class CoapTx: public CoapSocket {
  private:
    typedef struct {
      const char* uri;
      const char* dstIp;
      uint16_t dstPort;
      uint8_t tokenLen;
    } CoapTxConfig;

    typedef struct {
      char dstIp[40];
      int dstPort;

      struct {
        bool isAck;
        unsigned int expiredTime;
        uint8_t retransmitCounter;
      } AckMetadata;

      uint8_t messageId;
      uint8_t tokenLen;
      uint8_t token[8];

      CoapPacket packet;
    } Request;

    StaticList<CoapMessage, MAX_MSG_ENTRIES> msgList;
    StaticList<Request, MAX_MSG_ENTRIES> waitingResponseList;

    uint16_t setBuffer(CoapMessage &message, uint8_t *buffer);
    void transmitPacket(const char *ip, int port, CoapPacket packet);
    void insertArrayToBuffer(uint8_t *buffer, uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen);
    uint8_t encodeOptionField(uint16_t value, uint8_t out[3]);
    void decomposeUrlIntoOptions(CoapMessage &msg, const CoapTxConfig &cfg);
    void normalizeUriPath(char* path);

    template <typename T = const char*>
    void transmitLastMessage(CoapType type, CoapMethod method, T payload = "", uint16_t payloadLen = 0);

  public:
    using CoapSocket::CoapSocket;
    CoapTx& setConfig(const CoapTxConfig &cfg);
    void sendEmpty();

    #define DEFINE_COAP_METHOD(name, methodEnum) \
    template <typename T = const char*> \
    void name(CoapType type, T payload = "", uint16_t payloadLen = 0) { \
        this->transmitLastMessage(type, methodEnum, payload, payloadLen); \
    }

    DEFINE_COAP_METHOD(get,  COAP_GET)
    DEFINE_COAP_METHOD(post, COAP_POST)
    DEFINE_COAP_METHOD(put,  COAP_PUT)
    DEFINE_COAP_METHOD(del,  COAP_DELETE)

    #undef DEFINE_COAP_METHOD
};

class CoapRx: public CoapSocket {
  private:
    CircularQueue<CoapPacket, MAX_MSG_ENTRIES> bufferQueue;
    void parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen);
  public:
    using CoapSocket::CoapSocket;
    bool receiveMessage();
};

class Coap {
  private:
    CoapTx _tx;
    CoapRx _rx;
    CoapResource _resource;
  public:
    Coap(int port = DEFAULT_COAP_PORT);
    void start(WifiCfg wifiCfg);
    CoapTx& tx(); CoapRx& rx();

    //void handleBulkMessage();
    void addHandler(const char* path, uint8_t method, CoapHandler handler);
};
#endif
