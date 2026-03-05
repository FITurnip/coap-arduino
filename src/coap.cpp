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
  optionSize++;
}

const char* optionName(uint16_t num) {
    switch(num) {
        case 1: return "If-Match";
        case 3: return "Uri-Host";
        case 4: return "ETag";
        case 5: return "If-None-Match";
        case 7: return "Uri-Port";
        case 11: return "Uri-Path";
        case 12: return "Content-Format";
        case 15: return "Uri-Query";
        case 17: return "Accept";
        case 35: return "Proxy-Uri";
        case 39: return "Proxy-Scheme";
        case 60: return "Size1";
        default: return "Unknown";
    }
}
const char* getOptionName(uint16_t num) {
    switch(num) {
        case 1: return "If-Match";
        case 3: return "Uri-Host";
        case 4: return "ETag";
        case 5: return "If-None-Match";
        case 7: return "Uri-Port";
        case 11: return "Uri-Path";
        case 12: return "Content-Format";
        case 15: return "Uri-Query";
        case 17: return "Accept";
        case 35: return "Proxy-Uri";
        case 39: return "Proxy-Scheme";
        case 60: return "Size1";
        default: return "Unknown";
    }
}

void printOption(const CoapOpt &opt) {
  const char* name = getOptionName(opt.num);

  Serial.printf("%s\t: ", name); // fixed width 13 chars
  switch(opt.num) {
    case 3: // Uri-Host
    case 11: // Uri-Path
    case 15: // Uri-Query
    case 35: // Proxy-Uri
    case 39: // Proxy-Scheme
      for(uint16_t i=0;i<opt.len;i++)
        Serial.printf("%c", opt.val[i]);
      break;
    case 7: // Uri-Port
    case 12: // Content-Format
    case 17: // Accept:
    case 60: // Size1
      if(opt.len == 2)
        Serial.printf("%u", *((uint16_t*)opt.val));
      else if(opt.len == 4)
        Serial.printf("%u", *((uint32_t*)opt.val));
      break;
    default: // biner/opaque
      for(uint16_t i=0;i<opt.len;i++)
        Serial.printf("%02X ", opt.val[i]);
  }
  Serial.println();
}

void CoapMessage::diagnostic() {
  uint32_t token32 = 0;
  for(uint8_t i = 0; i < tokenLen; i++) token32 = (token32 << 8) | token[i];
  String msgType[4] = {"CON", "NON", "ACK", "RST"};

  uint8_t clsCode = code >> 5, detailCode = code & 0x1F;

  Serial.printf("CoAP %s\n", clsCode == 0 ? "REQUEST" : "RESPONSE");
  Serial.printf("Ver\t\t: %d.0\n", coapVersion);
  Serial.printf("Type\t\t: %s\n", msgType[type]);
  Serial.printf("Method\t\t: %d.%02d %s\n", clsCode, detailCode, this->getCoapCodeName(clsCode, detailCode));
  Serial.printf("MID\t\t: 0x%X\n", messageId);
  Serial.printf("Token\t\t: 0x%X\n", token32);


  for(uint8_t i = 0; i < optionSize; i++) {
    printOption(options[i]);
  }
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
