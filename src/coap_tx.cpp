#include "coap.h"
void CoapTx::insertArrayToBuffer(uint8_t *buffer, uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen) {
  if (entryLen == 0) return;
  for(uint16_t iEntry = 0; iEntry < entryLen && iBuffer < DEFAULT_BUFFER_SIZE; iEntry++) {
    buffer[iBuffer++] = entry[iEntry];
  }
}

uint8_t CoapTx::encodeOptionField(uint16_t value, uint8_t out[3]) {
  if (value < 13) {
    out[0] = value;
    return 1;
  } 
  if (value < 269) {
    out[0] = 13;
    out[1] = value - 13;
    return 2;
  }

  out[0] = 14;
  value -= 269;
  out[1] = (value >> 8) & 0xFF;
  out[2] = value & 0xFF;
  return 3;
}

uint16_t CoapTx::setBuffer(CoapMessage &message, uint8_t *buffer) {
  uint8_t tokenLen = message.tokenLen;
  if (tokenLen > 8) return 0;

  uint16_t iBuffer = 0;

  buffer[iBuffer++] =
    (message.coapVersion << 6) |
    ((message.type & 0x03) << 4) |
    (tokenLen & 0x0F);

  buffer[iBuffer++] = message.code;
  buffer[iBuffer++] = (message.messageId >> 8) & 0xFF;
  buffer[iBuffer++] = message.messageId & 0xFF;

  this->insertArrayToBuffer(buffer, iBuffer, message.token, tokenLen);

  uint16_t prevOptNum = 0;
  for(uint8_t iOption = 0; iOption < message.optionSize; iOption++) {
    uint16_t deltaTemp = message.options[iOption].num - prevOptNum;
    prevOptNum = message.options[iOption].num;

    uint8_t delta[3] = {0, 0, 0},  len[3] = {0, 0, 0};
    int deltaFieldSize = this->encodeOptionField(deltaTemp, delta);
    int lenFieldSize = this->encodeOptionField(message.options[iOption].len, len);

    buffer[iBuffer++] = (delta[0] << 4) | (len[0] & 0x0F);
    for(uint8_t i = 1; i < deltaFieldSize; i++) buffer[iBuffer++] = delta[i];
    for(uint8_t i = 1; i < lenFieldSize; i++) buffer[iBuffer++] = len[i];
    this->insertArrayToBuffer(buffer, iBuffer, message.options[iOption].val, message.options[iOption].len);
  }

  buffer[iBuffer++] = 0xFF;

  if (message.payloadLen > 0) {
    if (iBuffer + 1 >= DEFAULT_BUFFER_SIZE) return 0;
    this->insertArrayToBuffer(buffer, iBuffer, message.payload, message.payloadLen);
  }

  return iBuffer;
}

void CoapTx::transmitPacket(const char *ip, int port, CoapPacket packet) {
  this->_transmit(ip, port, packet);
}

void CoapTx::normalizeUriPath(char* path) {
  if(!path) return;

  char* p;

  // 1. handle double slash "//"
  while ((p = strstr(path, "//")) != NULL) {
    memmove(p, p + 1, strlen(p + 1) + 1);
  }

  // 2. handle "/./"
  while ((p = strstr(path, "/./")) != NULL) {
    memmove(p + 1, p + 3, strlen(p + 3) + 1);
  }

  // 3. handle "/../"
  while ((p = strstr(path, "/../")) != NULL) {
    char* start = p;

    // cari slash sebelumnya
    while (start > path && *(start - 1) != '/')
      start--;

    if (start > path)
      start--;

    memmove(start, p + 4, strlen(p + 4) + 1);
  }
}

void CoapTx::decomposeUrlIntoOptions(CoapMessage &msg, const CoapTxConfig &cfg) {
  if(!cfg.uri) return;

  char uri[256];
  strncpy(uri, cfg.uri, sizeof(uri)-1);
  uri[sizeof(uri)-1] = 0;

  // step 1: must be absllute uri
  char* schemeEnd = strstr(uri, "://");
  if(!schemeEnd) return;

  int schemeLen = schemeEnd - uri;
  char* hostStart = schemeEnd + 3; // shift ://

  char* path = strchr(hostStart, '/');
  if(!path) path = hostStart + strlen(hostStart);

  // step 2: resolve reformattedUri
  if(path) this->normalizeUriPath(path);

  // step 3: scheme must be coap or coaps
  bool isCoapScheme = false;
  uint16_t curDstPort = 0;
  if(schemeLen == 4 && strncasecmp(uri, "coap", 4) == 0) curDstPort = 5683;
  else if(schemeLen == 5 && strncasecmp(uri, "coaps", 5) == 0) curDstPort = 5684;
  isCoapScheme = curDstPort != 0;
  if(!isCoapScheme) return;

  // step 4: must not contain fragment
  char* frag = strchr(schemeEnd, '#');
  if(frag) return;

  // step 5: add host to option if dstIp != host. percent-econdings convertion is skipped
  char* hostEnd = hostStart;
  while(*hostEnd && *hostEnd != '/' && *hostEnd != ':') hostEnd++;

  int hostLen = hostEnd - hostStart;
  if(hostLen != (int)strlen(cfg.dstIp) || strncmp(hostStart, cfg.dstIp, hostLen) != 0) {
    uint8_t hostVal[40];
    int copyLen = hostLen < 40 ? hostLen : 39;
    for(int i = 0; i < copyLen; i++) hostVal[i] = tolower((unsigned char) hostStart[i]);
    msg.addOption(3, (uint16_t) copyLen, hostVal);
  }

  // step 6: if uri didnt contain port, port is default port the scheme
  if(!hostEnd) hostEnd = hostStart + strlen(hostStart);
  char* portStart = strchr(hostStart, ':');
  if(portStart && portStart < hostEnd) curDstPort = atoi(portStart + 1);

  // step 7: add port to option if not equal to dstPort
  if(curDstPort != cfg.dstPort) msg.addOption(7, sizeof(uint16_t), (uint8_t*)&curDstPort);

  // step 8: add path to option if exist
  for(char* seg = path; *seg && *seg != '?'; ) {
    if(*seg == '/') { seg++; continue; }
    char* next = strchr(seg, '/');
    if(!next || next > strchr(path, '?')) next = strchr(path, '?');
    size_t len = next ? next - seg : strlen(seg);
    if(len) msg.addOption(11, len, (uint8_t*)seg);
    seg = next ? next : nullptr;
  }

  // step 9: add query if exist
  char* queryStart = strchr(path, '?');
  if(queryStart) {
    char* arg = queryStart + 1;
    while(arg && *arg) {
      char* next = strchr(arg, '&');
      size_t len = next ? (size_t)(next - arg) : strlen(arg);
      if(len) msg.addOption(15, len, (uint8_t*)arg);
      arg = next ? next + 1 : nullptr;
    }
  }
}

CoapTx& CoapTx::setConfig(const CoapTxConfig &cfg) {
  unsigned int lastMsgIdx = msgList.length() - 1;
  if(msgList.isFull()) return *this;
  if(cfg.tokenLen > 8) return *this;
  CoapMessage newMsg;
  newMsg.messageId = lastMsgIdx >= 0 ? newMsg.messageId + 1 : random(0, 65536);
  newMsg.tokenLen = cfg.tokenLen;
  for(uint8_t i = 0; i < newMsg.tokenLen; i++) newMsg.token[i] = random(0, 256);
  
  if(cfg.dstIp) {
    strncpy(newMsg.dstIp, cfg.dstIp, sizeof(newMsg.dstIp)-1);
    newMsg.dstIp[sizeof(newMsg.dstIp) -1] = '\0';
  }
  newMsg.dstPort = cfg.dstPort;

  this->decomposeUrlIntoOptions(newMsg, cfg);

  msgList.push(newMsg);

  return *this;
}

template <typename T>
void CoapTx::transmitLastMessage(CoapType type, CoapMethod method, T payload, uint16_t payloadLen) {
  unsigned int lastMsgIdx = msgList.length() - 1;
  if(lastMsgIdx == -1) return;
  CoapMessage &msg = msgList[lastMsgIdx];

  if (!msg.dstIp || !msg.dstPort) {
    Serial.println("no ip:port");
    return;
  }

  msg.type = type;
  msg.code = method;

  if(method != COAP_EMPTY && payloadLen > 0) {
    static_assert(
      std::is_same<T, const char*>::value || 
      std::is_same<T, uint8_t*>::value || 
      std::is_same<T, char*>::value,
      "Tipe data tidak didukung!"
    );
    msg.payloadLen = payloadLen;
    memcpy(msg.payload, payload, msg.payloadLen);
    if constexpr (std::is_same<T, const char*>::value || std::is_same<T, char*>::value) {
      if(msg.payload[msg.payloadLen] != '\0') msg.payload[msg.payloadLen++] = '\0';
    }
  }

  Request request;
  strncpy(request.dstIp, msg.dstIp, sizeof(request.dstIp));
  request.dstPort = msg.dstPort,
  request.AckMetadata.isAck = type,
  request.messageId = msg.messageId,
  request.tokenLen = msg.tokenLen;
  memcpy(request.token, msg.token, request.tokenLen);

  CoapPacket &packet = request.packet;
  packet.size = this->setBuffer(msg, packet.data);
  if(packet.size > DEFAULT_BUFFER_SIZE) return;
  msg.diagnostic();

  waitingResponseList.push(request);
  msgList.remove(lastMsgIdx);
  this->transmitPacket(request.dstIp, request.dstPort, packet);
}

void CoapTx::sendEmpty() {
  this->transmitLastMessage(COAP_NON, COAP_GET);
}

