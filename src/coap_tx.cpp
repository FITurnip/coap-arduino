#include "coap.h"
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

void CoapTx::setBuffer(CoapMessage &msg, CoapBuffer &buffer) {
  buffer.size = 0;
  // size validation
  if (msg.token.size > 8) return;
  uint16_t bufferSize = 4 + msg.token.size, prevOptNum = 0;
  for(uint16_t i = 0; i < msg.optionSize; i++) {
    CoapOpt* curOpt = &msg.options[i];
    uint16_t deltaNum = curOpt->num - prevOptNum;
    prevOptNum = curOpt->num;

    uint8_t delta[3] = {0, 0, 0},  len[3] = {0, 0, 0};
    int deltaFieldSize = this->encodeOptionField(deltaNum, delta);
    int lenFieldSize = this->encodeOptionField(curOpt->len, len);

    bufferSize += 1 + (deltaFieldSize - 1) + (lenFieldSize - 1) + msg.options[i].len;
  }
  uint8_t* writeHead = buffer.data;
  if (msg.payload.size > 0) {
    if (msg.optionSize > 0) writeHead++; // Marker 0xFF
    buffer.size += msg.payload.size;
  }
  if(buffer.size > DEFAULT_BUFFER_SIZE) return;


  // header
  *writeHead++ = (msg.coapVersion << 6) | ((msg.type & 0x03) << 4) | (msg.token.size & 0x0F);
  *writeHead++ = msg.code;
  *writeHead++ = (msg.messageId >> 8) & 0xFF;
  *writeHead++ = msg.messageId & 0xFF;

  // token
  memcpy(writeHead, msg.token.data, msg.token.size);
  writeHead += msg.token.size;

  // options
  prevOptNum = 0;
  CoapOpt *readOpt = msg.options, *endOpt = msg.options + msg.optionSize;
  while(readOpt < endOpt) {
    uint16_t deltaNum = readOpt->num - prevOptNum;
    prevOptNum = readOpt->num;

    uint8_t delta[3] = {0, 0, 0},  len[3] = {0, 0, 0};
    int deltaFieldSize = this->encodeOptionField(deltaNum, delta);
    int lenFieldSize = this->encodeOptionField(readOpt->len, len);

    *writeHead++ = (delta[0] << 4) | (len[0] & 0x0F);
    for(uint8_t i = 1; i < deltaFieldSize; i++) *writeHead++ = delta[i];
    for(uint8_t i = 1; i < lenFieldSize; i++) *writeHead++ = len[i];
    memcpy(writeHead, readOpt->val, readOpt->len);
    writeHead += readOpt->len;
    readOpt++;
  }

  // payload
  if(msg.payload.size > 0) {
    if(msg.optionSize > 0) *writeHead++ = 0xFF; // marker
    memcpy(writeHead, msg.payload.data, msg.payload.size);
  }

  buffer.size = bufferSize;
}

void CoapTx::transmitPacket(CoapTransactionContext &transactionContext) {
  this->_transmit(transactionContext);
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
  snprintf(uri, sizeof(uri), "%s", cfg.uri);

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
  if (hostLen <= 0) return;

  char tempHost[64];
  int copyLen = (hostLen < (int)sizeof(tempHost) - 1) ? hostLen : (int)sizeof(tempHost) - 1;
  
  memcpy(tempHost, hostStart, copyLen);
  tempHost[copyLen] = '\0';

  IPAddress tempIp;
  bool isNumericIp = tempIp.fromString(tempHost);

  if (!isNumericIp || tempIp != cfg.dstIp) {
    uint8_t hostVal[64];
    for(int i = 0; i < copyLen; i++) {
      hostVal[i] = (uint8_t)tolower((unsigned char)tempHost[i]);
    }
    
    msg.addOption(URI_HOST, (uint16_t)copyLen, hostVal);
  }

  // step 6: if uri didnt contain port, port is default port the scheme
  if(!hostEnd) hostEnd = hostStart + strlen(hostStart);
  char* portStart = strchr(hostStart, ':');
  if(portStart && portStart < hostEnd) curDstPort = atoi(portStart + 1);

  // step 7: add port to option if not equal to dstPort
  if(curDstPort != cfg.dstPort) msg.addOption(URI_PORT, sizeof(uint16_t), (uint8_t*)&curDstPort);

  // step 8: add path to option if exist
  for(char* seg = path; *seg && *seg != '?'; ) {
    if(*seg == '/') { seg++; continue; }
    char* next = strchr(seg, '/');
    if(!next || next > strchr(path, '?')) next = strchr(path, '?');
    size_t len = next ? next - seg : strlen(seg);
    if(len) msg.addOption(URI_PATH, len, (uint8_t*)seg);
    seg = next ? next : nullptr;
  }

  // step 9: add query if exist
  char* queryStart = strchr(path, '?');
  if(queryStart) {
    char* arg = queryStart + 1;
    while(arg && *arg) {
      char* next = strchr(arg, '&');
      size_t len = next ? (size_t)(next - arg) : strlen(arg);
      if(len) msg.addOption(URI_QUERY, len, (uint8_t*)arg);
      arg = next ? next + 1 : nullptr;
    }
  }
}

CoapTx& CoapTx::setConfig(const CoapTxConfig &cfg) {
  if(msgList.isFull()) return *this;
  if(cfg.tokenLen > 8) return *this;
  unsigned int totalMsgList = msgList.length();

  CoapMessage newMsg;
  newMsg.messageId = totalMsgList >= 0 ? newMsg.messageId + 1 : random(0, 65536);
  newMsg.token.size = cfg.tokenLen;
  for(uint8_t i = 0; i < newMsg.token.size; i++) newMsg.token.data[i] = random(0, 256);
  
  newMsg.dstIp = cfg.dstIp;
  newMsg.dstPort = cfg.dstPort;
  this->decomposeUrlIntoOptions(newMsg, cfg);

  msgList.push(newMsg);

  return *this;
}

template <typename T>
void CoapTx::transmitLastMsg(CoapType type, CoapMethod method, T payload, uint16_t payloadLen) {
  unsigned int msgListSize = msgList.length();
  if(msgListSize == 0) return;
  CoapMessage &msg = msgList[msgListSize - 1];

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
    msg.payload.size = payloadLen;
    memcpy(msg.payload.data, payload, msg.payload.size);
  }

  CoapTransactionContext transactionContext;
  transactionContext.type = type;
  transactionContext.code = method;
  transactionContext.messageId = msg.messageId;
  transactionContext.token = msg.token;

  this->setBuffer(msg, transactionContext.buffer);
  if(transactionContext.buffer.size == 0) return;
  msg.print();

  transactionContext.dstIp = msg.dstIp;
  transactionContext.dstPort = msg.dstPort,

  waitingResponseList.push(transactionContext);
  msgList.remove(msgListSize - 1);
  this->transmitPacket(transactionContext);
}

void CoapTx::sendEmpty() {
  this->transmitLastMsg(COAP_NON, COAP_GET);
}

void CoapTx::sendResponse(CoapTransactionContext &transactionContext) {
  this->transmitPacket(transactionContext);
}

