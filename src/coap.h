#ifndef __COAP_H__
#define __COAP_H__
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "../lib/CircularQueue/circular_queue.h"
#include "../lib/StaticList/static_list.h"
#include "coap_config.h"

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
  CoapOptNum num;
  uint16_t len;
  uint8_t val[MAX_OPTION_VAL_LEN];
} CoapOpt;

class CoapMessage {
private:
  const char* getCoapCodeName(uint8_t cls, uint8_t detail);
  void printOption(const CoapOpt &opt);

public:
  uint8_t  coapVersion = 1;
  CoapType  type;
  uint8_t  code;
  uint16_t messageId;
  CoapData<8> token;
  CoapOpt options[MAX_OPTIONS];
  uint8_t optionSize = 0;
  CoapData<DEFAULT_PAYLOAD_SIZE> payload;

  IPAddress dstIp;
  int dstPort;

  void addOption(CoapOptNum num, uint16_t len, const uint8_t *val);
  void print();
};

typedef CoapTransactionContext (*CoapHandler)(CoapMessage &msg);
class CoapResource {
private:
    char* path;
    CoapHandler handlers[5];
    CoapResource* children[10];
    uint8_t childCount;

    CoapResource* findOrCreateChild(const char* name);
    CoapResource* findChild(const char* name);

public:
    CoapResource(const char* p = nullptr);
    void addHandler(const char* path, uint8_t method, CoapHandler handler);
    CoapTransactionContext handleRequest(CoapMessage& msg, CoapTransactionContext &reqContext);
};

class CoapSocket {
  protected:
    WiFiUDP _udp;
    int _port;
    
    bool _transmit(CoapTransactionContext& transactionContext);
    bool _receive(CoapTransactionContext& transactionContext);
  public:
    CoapSocket(int port = DEFAULT_COAP_PORT);
    void start(WifiCfg wifiCfg);

    WiFiUDP* getTransport();
    void setTransport(WiFiUDP* transport);
};

class CoapTx: public CoapSocket {
  private:
    StaticList<CoapMessage, MAX_MSG_ENTRIES> msgList;
    StaticList<CoapTransactionContext, MAX_MSG_ENTRIES> waitingResponseList;

    void setBuffer(CoapMessage &message, CoapBuffer &buffer);
    void transmitPacket(CoapTransactionContext &transactionContext);
    uint8_t encodeOptionField(uint16_t value, uint8_t out[3]);
    void decomposeUrlIntoOptions(CoapMessage &msg, const CoapTxConfig &cfg);
    void normalizeUriPath(char* path);

    template <typename T = const char*>
    void transmitLastMsg(CoapType type, CoapMethod method, T payload = "", uint16_t payloadLen = 0);

  public:
    using CoapSocket::CoapSocket;
    CoapTx& setConfig(const CoapTxConfig &cfg);
    void sendEmpty();

    #define DEFINE_COAP_METHOD(name, methodEnum) \
    template <typename T = const char*> \
    void name(CoapType type, T payload = "", uint16_t payloadLen = 0) { \
        this->transmitLastMsg(type, methodEnum, payload, payloadLen); \
    }

    DEFINE_COAP_METHOD(get,  COAP_GET)
    DEFINE_COAP_METHOD(post, COAP_POST)
    DEFINE_COAP_METHOD(put,  COAP_PUT)
    DEFINE_COAP_METHOD(del,  COAP_DELETE)

    #undef DEFINE_COAP_METHOD

    void sendResponse(CoapTransactionContext &transactionContext);
};

class CoapRx: public CoapSocket {
  private:
    CircularQueue<CoapTransactionContext, MAX_MSG_ENTRIES> transactionQueue;
    void parseReceived(CoapMessage &msg, CoapBuffer &buffer);
  public:
    using CoapSocket::CoapSocket;
    bool receiveMessage();
    bool shiftMessage(CoapMessage &msg, CoapTransactionContext &transactionContext);
};

class Coap {
  private:
    CoapTx _tx;
    CoapRx _rx;
  public:
    Coap(int port = DEFAULT_COAP_PORT);
    void start(WifiCfg wifiCfg);
    CoapTx& tx(); CoapRx& rx();

    CoapResource resource;
    void addHandler(const char* path, uint8_t method, CoapHandler handler);
    void handleReceivedMsg();
};
#endif
