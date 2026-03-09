#include "coap.h"
void CoapRx::parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen) {
  // --- Header ---
  msg.coapVersion   = buffer[0] >> 6;
  msg.type          = (buffer[0] >> 4) & 0x03;
  msg.tokenLen      = buffer[0] & 0x0F;
  msg.code          = buffer[1];
  msg.messageId     = (buffer[2] << 8) | buffer[3];

  if (msg.tokenLen > 8) return;

  uint8_t iBuffer = 4;

  // --- Token ---
  if (msg.tokenLen > 0) {
    memcpy(msg.token, &buffer[iBuffer], msg.tokenLen);
    iBuffer += msg.tokenLen;
  }

  // --- Options ---
  uint16_t prevOptionNum = 0;
  while (iBuffer < bufferLen) {
    if(buffer[iBuffer] == 0xFF) {
      iBuffer++;
      break;
    }

    uint8_t   optField  = buffer[iBuffer++];
    uint16_t  optDelta  = (optField >> 4) & 0x0F;
    uint16_t  optLen    = optField & 0x0F;

    // extended delta
    if(optDelta == 13) {
      if(iBuffer >= bufferLen) break; // safety
      optDelta = 13 + buffer[iBuffer++];
    } else if(optDelta == 14) {
      if(iBuffer + 1 >= bufferLen) break;
      optDelta = 269 + (buffer[iBuffer] << 8 | buffer[iBuffer + 1]);
      iBuffer += 2;
    } else if(optDelta == 15) {
      // reserved, invalid
      break;
    }

    // extended length
    if(optLen == 13) {
      if(iBuffer >= bufferLen) break;
      optLen = 13 + buffer[iBuffer++];
    } else if(optLen == 14) {
      if(iBuffer + 1 >= bufferLen) break;
      optLen = 269 + (buffer[iBuffer] << 8 | buffer[iBuffer + 1]);
      iBuffer += 2;
    } else if(optLen == 15) {
      // reserved, invalid
      break;
    }

    uint16_t optionNum = prevOptionNum + optDelta;
    prevOptionNum = optionNum;

    if(iBuffer + optLen > bufferLen) break;

    uint8_t* val = (uint8_t*)malloc(optLen);
    memcpy(val, buffer + iBuffer, optLen);
    iBuffer += optLen;
    msg.addOption(optionNum, optLen, val);
  }

  // --- Payload ---
  if(buffer[iBuffer] == 0xFF) {
    iBuffer++;
  }
  if (iBuffer < bufferLen) {
    msg.payloadLen  = bufferLen - iBuffer;
    memcpy(msg.payload, &buffer[iBuffer++], msg.payloadLen);
  } else {
    msg.payloadLen  = 0;
  }
}

bool CoapRx::receiveMessage() {
  bool ok = this->_receive();
  if (ok) bufferQueue.push(lastPacketReceived);
  lastPacketReceived.size = 0;
  return ok;
}

bool CoapRx::shiftMessage(CoapMessage &msg) {
  if(bufferQueue.empty()) return false;
  CoapPacket packet = bufferQueue.front();
  bufferQueue.pop();
  this->parseReceived(msg, packet.data, packet.size);
  msg.print();
  return true;
}

CoapResource::CoapResource(const char* p) {
  if(p) {
    size_t len = strlen(p) + 1;
    path = (char*)malloc(len);
    if(path) strcpy(path, p);
  } else {
    path = nullptr;
  }

  for(int i=0;i<10;i++) children[i] = nullptr;
  for(int i=0;i<4;i++) handlers[i] = nullptr;
}

CoapResource* CoapResource::findChild(const char* name) {
  for(uint8_t i=0;i<childCount;i++) {
    if(children[i]->path && strcmp(children[i]->path, name) == 0) {
      return children[i];
    }
  }
  return nullptr;
}

CoapResource* CoapResource::findOrCreateChild(const char* name) {
    CoapResource* child = findChild(name);
    if(child) return child;
    if(childCount >= 10) return nullptr;

    CoapResource* newChild = new CoapResource(name);
    children[childCount++] = newChild;
    return newChild;
}

void CoapResource::addHandler(const char* fullPath, uint8_t method, CoapHandler handler) {
    if(!fullPath || !handler || method >= 4) return;

    char* pathCopy = (char*)malloc(strlen(fullPath)+1);
    if(!pathCopy) return;
    strcpy(pathCopy, fullPath);

    char* token = strtok(pathCopy, "/");
    CoapResource* node = this;

    while(token) {
        node = node->findOrCreateChild(token);
        if(!node) break;
        token = strtok(nullptr, "/");
    }

    if(node) node->handlers[method] = handler;
    free(pathCopy);
}

void CoapResource::handleRequest(const char* fullPath, uint8_t method, CoapMessage& req) {
  if(!fullPath || method >= 4) return;

  char pathCopy[strlen(fullPath) + 1];

  strcpy(pathCopy, fullPath);
  pathCopy[sizeof(pathCopy) - 1] = '\0';

  char* token = strtok(pathCopy, "/");
  CoapResource* node = this;

  while(token && node) {
    node = node->findChild(token);
    token = strtok(nullptr, "/");
  }

  if(node && node->handlers[method]) {
    CoapMessage resMsg;
    resMsg = node->handlers[method](req);
  } else {
    Serial.println("4.04 NOT_FOUND");
  }
}

void CoapRx::handleReceivedMsg() {
  CoapMessage msg;
  bool isNotEmpty = this->shiftMessage(msg);
  if(!isNotEmpty) return;
  uint8_t clsCode = msg.code >> 5, detailCode = msg.code & 0x1F;

  if(clsCode == 0 && detailCode > 0) {
    uint16_t uriIndex = 0;
    while(uriIndex != msg.optionSize && msg.options[uriIndex].num != 11) uriIndex++;

    size_t totalLen = 0;
    uint16_t tmpIndex = uriIndex;
    while(tmpIndex != msg.optionSize && msg.options[tmpIndex].num == 11) {
        totalLen += msg.options[tmpIndex].len;
        tmpIndex++;
    }
    uint16_t segmentCount = tmpIndex - uriIndex;
    if(segmentCount > 1) totalLen += segmentCount - 1;

    char path[totalLen + 1];
    uint16_t currentPos = 0;
    path[0] = '\0';
    while(uriIndex != msg.optionSize && msg.options[uriIndex].num == 11) {
      if(currentPos > 0 && currentPos < sizeof(path) - 1) {
        path[currentPos++] = '/';
        path[currentPos] = '\0';
      }

      uint16_t segmentLen = msg.options[uriIndex].len;
      if(currentPos + segmentLen < sizeof(path)) {
        memcpy(&path[currentPos], msg.options[uriIndex].val, segmentLen);
        currentPos += segmentLen;
        path[currentPos] = '\0'; // Selalu tutup dengan null terminator
      }
      uriIndex++;
    }

    CoapMessage msgResponse;
    this->resource.handleRequest(path, detailCode, msg);
  }
}
