#ifndef __COAP_CONFIG_H__
#define __COAP_CONFIG_H__

#define DEFAULT_COAP_PORT 5683
#define DEFAULT_BUFFER_SIZE 1152
#define DEFAULT_PAYLOAD_SIZE 1024
#define MAX_MSG_ENTRIES 10
#define MAX_OPTION_VAL_LEN 64
#define MAX_OPTIONS 32

#define ACK_TIMEOUT       2000
#define ACK_RANDOM_FACTOR 1.5
#define MAX_RETRANSMIT    4
#define NSTART            1       // skipped
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
  const char *ssid, *password;
  IPAddress localIP, gateway, subnet;
} WifiCfg;

template <size_t N>
struct CoapData {
    uint8_t data[N];
    uint16_t size = 0;
};
using CoapBuffer  = CoapData<DEFAULT_BUFFER_SIZE>;
using CoapPayload = CoapData<DEFAULT_PAYLOAD_SIZE>;

struct CoapTransactionContext {
  uint8_t type = 0;
  uint8_t code = 1;
  uint16_t messageId = 0;
  uint8_t tokenLen = 8, token[8];

  CoapBuffer buffer;

  IPAddress dstIp;
  int dstPort;
}; 

#endif
