#include "coap.h"
void CoapTx::insertArrayToBuffer(uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen) {
  if (entryLen == 0) return;
  for(uint16_t iEntry = 0; iEntry < entryLen && iBuffer < this->_maxBufferSize; iEntry++) {
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

uint16_t CoapTx::setBuffer() {
  if (this->_maxBufferSize < 4) return 0;

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

  this->insertArrayToBuffer(iBuffer, message.token, tokenLen);

  Serial.printf("optionsize: %d\n", message.optionSize);
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
    this->insertArrayToBuffer(iBuffer, message.options[iOption].val, message.options[iOption].len);
  }

  buffer[iBuffer++] = 0xFF;

  if (message.payloadLen > 0) {
    if (iBuffer + 1 >= this->_maxBufferSize) return 0;
    this->insertArrayToBuffer(iBuffer, message.payload, message.payloadLen);
  }

  return iBuffer;
}

void CoapTx::transmitPacket(const char *ip, int port, uint16_t actualBufferSize) {
  yield();
  int ok = this->_udp->beginPacket(ip, port);
  if (!ok) {
    Serial.println("beginPacket failed");
    return;
  }
  this->_udp->write(buffer, actualBufferSize);
  this->_udp->endPacket();
  yield();
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

void CoapTx::setDstAddress(const char* ip, int port) {
    if(this->dstIp) {
        free((void*)this->dstIp);
        this->dstIp = nullptr;
    }

    if(!ip) return;
    size_t len = strlen(ip) + 1;
    char* copy = (char*)malloc(len);
    if(!copy) return;
    memcpy(copy, ip, len);
    this->dstIp = copy;
    this->dstPort = port;
}

void CoapTx::decomposeUrlIntoOptions(const char* destUri) {
  if(!destUri) return;

  size_t len = strlen(destUri) + 1;
  char* uri = (char*)malloc(len);
  if(!uri) return;
  strcpy(uri, destUri);

  // step 1: must be absllute uri
  char* reformattedUri = strstr(uri, "://");
  if(!reformattedUri) {
    free(uri);
    return;
  }
  char* schemeEnd = reformattedUri;
  int schemeLen = schemeEnd - uri;

  reformattedUri += 3; // shift ://
  char* path = strchr(reformattedUri, '/');

  if(!path) path = reformattedUri + strlen(reformattedUri);

  // step 2: resolve reformattedUri
  if(path) this->normalizeUriPath(path);

  // step 3: scheme must be coap or coaps
  bool isCoapScheme = false;
  uint16_t curDstPort = 0;
  if(schemeLen == 4 && strncasecmp(uri, "coap", 4) == 0) curDstPort = 5683;
  else if(schemeLen == 5 && strncasecmp(uri, "coaps", 5) == 0) curDstPort = 5684;
  isCoapScheme = curDstPort != 0;
  if(!isCoapScheme) {
    free(uri);
    return;
  }

  // step 4: must not contain fragment
  char* frag = strchr(reformattedUri, '#');
  if(frag) {
    free(uri);
    return;
  }

  // step 5: add host to option if dstIp != host. percent-econdings convertion is skipped
  char *hostStart = schemeEnd + 3;
  char *hostEnd = hostStart;
  while(*hostEnd && *hostEnd != '/' && *hostEnd != ':') hostEnd++;

  int hostLen = hostEnd - hostStart, dstLen = strlen(dstIp);
  if(hostStart[0] == '[' && hostStart[hostLen-1] == ']') {
    hostStart++;
    hostLen -= 2;
  }

  if(hostLen != dstLen || strncmp(hostStart, dstIp, hostLen) != 0) {
    uint8_t* hostCopy = (uint8_t*)malloc(hostLen);
    if(hostCopy) {
      for(int i = 0; i < hostLen; i++) hostCopy[i] = tolower((unsigned char) hostStart[i]);
      message.addOption(3, (uint16_t) hostLen, hostCopy);
    }
  }

  // step 6: if uri didnt contain port, port is default port the scheme
  char* portStart = strchr(hostStart, ':');
  if(portStart && portStart < path) {
    curDstPort = atoi(portStart + 1);
  }

  // step 7: add port to option if not equal to dstPort
  if(curDstPort != dstPort) {
    uint16_t portVal = curDstPort;
    message.addOption(7, sizeof(uint16_t), (uint8_t*)&portVal);
  }

  // step 8: add path to option if exist
  char* queryStart = strchr(path, '?');
  if(path && !(path[0]=='/' && path[1]=='\0')) {
    for(char* seg = path; *seg && seg < queryStart; ) {
      if(*seg == '/') seg++;
      if(!*seg) break;

      char* next = strchr(seg, '/');
      if(!next || (queryStart && next > queryStart)) next = queryStart;

      size_t len = next - seg;
      if(len) message.addOption(11, (uint16_t)len, (uint8_t*)seg);

      seg = next ? next : NULL;
    }
  }

  // step 9: add query if exist
  if(queryStart && *(queryStart+1)) {
    char* arg = queryStart + 1;
    char* next;
    while(arg && *arg) {
      next = strchr(arg, '&');
      size_t len = next ? (size_t)(next - arg) : strlen(arg);

      if(len) {
        message.addOption(15, (uint16_t)len, (uint8_t*)arg);
      }
      arg = next ? next + 1 : NULL;
    }
  }
  free(uri);
}

void CoapTx::init(const char* ip, int port, uint8_t tokenLen) {
  beginConnection();

  // init header
  if(tokenLen > 8) return;
  message.coapVersion = 1;
  message.type = 0;
  
  message.code = 0x01;
  message.messageId = random(0, 65536);
  message.payloadLen = 0;
  message.tokenLen = tokenLen;

  this->setDstAddress(ip, port);
}

void CoapTx::setMessage(const char* uri) {
  // regenerate token
  if(message.tokenLen > 8) return;
  for(uint8_t i = 0; i < message.tokenLen; i++) message.token[i] = random(0, 256);

  // wrap messageId
  message.messageId++;

  // add option
  message.optionSize = 0;
  this->decomposeUrlIntoOptions(uri);
}

void CoapTx::transmitMessage() {
  if (!dstIp || !dstPort) {
    Serial.println("no ip:port");
    return;
  }

  uint16_t actualBufferSize = this->setBuffer();
  if(actualBufferSize > DEFAULT_BUFFER_SIZE) return;
  message.diagnostic();
  this->transmitPacket(dstIp, dstPort, actualBufferSize);
}
