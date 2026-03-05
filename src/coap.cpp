#include "coap.h"

CoapBase::CoapBase(UDP &udp, int port, int maxBufferSize) : _udp(&udp), _port(port), _maxBufferSize(maxBufferSize) {
  buffer = (uint8_t*) malloc(this->_maxBufferSize);
}


void CoapBase::beginConnection() {
    this->_udp->begin(this->_port);
}

const char* CoapMessage::getCoapCodeName(uint8_t cls, uint8_t detail) {
  if (cls == 0) {
    if (detail == 0) return "EMPTY MESSAGE";
    if (detail == 1) return "GET";
    if (detail == 2) return "POST";
    if (detail == 3) return "PUT";
    if (detail == 4) return "DELETE";
  }

  if (cls == 2) {
    if (detail == 1) return "CREATED";
    if (detail == 2) return "DELETED";
    if (detail == 3) return "VALID";
    if (detail == 4) return "CHANGED";
    if (detail == 5) return "CONTENT";
  }

  if (cls == 4) {
    if (detail == 0) return "BAD_REQUEST";
    if (detail == 1) return "UNAUTHORIZED";
    if (detail == 4) return "NOT_FOUND";
    if (detail == 5) return "METHOD_NOT_ALLOWED";
  }

  if (cls == 5) {
    if (detail == 0) return "INTERNAL_SERVER_ERROR";
    if (detail == 1) return "NOT_IMPLEMENTED";
    if (detail == 3) return "SERVICE_UNAVAILABLE";
  }

  return "UNKNOWN";
}

void CoapMessage::addOption(uint16_t num, uint16_t len, uint8_t *val) {
  options[optionSize].num = num;
  options[optionSize].len = len;
  options[optionSize].val = val;
}

void CoapMessage::diagnostic() {
  uint32_t token32 = 0;
  for(uint8_t i = 0; i < tokenLen; i++) token32 = (token32 << 8) | token[i];
  String msgType[4] = {"CON", "NON", "ACK", "RST"};

  uint8_t clsCode = code >> 5, detailCode = code & 0x1F;

  Serial.printf("CoAP %s\n", clsCode == 0 ? "REQUEST" : "RESPONSE");
  Serial.printf("Ver\t: %d.0\n", coapVersion);
  Serial.printf("Type\t: %s\n", msgType[type]);
  Serial.printf("Method\t: %d.%02d %s\n", clsCode, detailCode, this->getCoapCodeName(clsCode, detailCode));
  Serial.printf("MID\t: 0x%X\n", messageId);
  Serial.printf("Token\t: 0x%X\n", token32);
  Serial.print("\n");
}

void CoapUri::add(String uri, void (*callback)(CoapMessage &msg)) {
  if(this->counter < MAX_URI) return;
  this->uri[counter] = uri;
  this->handler[counter++] = callback;
}

void CoapUri::handle(String uri, CoapMessage &msg) {
  bool found = false;
  int indexUri = 0;
  while(!found && indexUri < this->counter) {
    found = (uri == this->uri[indexUri++]);
  }
  if(found) this->handler[indexUri](msg);
}
