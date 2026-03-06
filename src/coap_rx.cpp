#include "coap.h"
CoapRx::CoapRx(UDP &udp, int port, int bufferSize)
    : CoapBase(udp, port, bufferSize), messageQueue(COAP_MSG_QUEUE_SIZE), resource("")
{
}

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

    msg.options[msg.optionSize].num = optionNum;
    msg.options[msg.optionSize].len = optLen;
    msg.options[msg.optionSize].val = val;
    msg.optionSize++;
  }

  // --- Payload ---
  if (iBuffer < bufferLen && buffer[iBuffer] == 0xFF) {
    msg.payload     = &buffer[iBuffer++];
    msg.payloadLen  = bufferLen - iBuffer;
  } else {
    msg.payload     = nullptr;
    msg.payloadLen  = 0;
  }
}

bool CoapRx::receiveMessage() {
  uint16_t bufferSize = this->_udp->parsePacket();
  if (bufferSize < 4) return false; // header only

  uint8_t buffer[bufferSize];
  this->_udp->read(buffer, bufferSize);
  messageQueue.enqueue(buffer, bufferSize);
  return true;
}

void CoapRx::handleBulkMessage() {
  while(!this->messageQueue.isEmpty()) {
    int actualBufferSize = _maxBufferSize; // temp
    this->messageQueue.dequeue(buffer, actualBufferSize);

    if(actualBufferSize > DEFAULT_BUFFER_SIZE) return;

    CoapMessage msg;
    this->parseReceived(msg, buffer, actualBufferSize);
    msg.diagnostic();

    uint8_t clsCode = msg.code >> 5, detailCode = msg.code & 0x1F;
    if(clsCode == 0 && detailCode > 0) {
      uint16_t uriIndex = 0;
      while(uriIndex != msg.optionSize && msg.options[uriIndex].num != 11) uriIndex++;

      /*String path = "";
      while(uriIndex != msg.optionSize && msg.options[uriIndex].num == 11) {
        if(path != "") path += "/";
        path += (char*) msg.options[uriIndex].val;
        //path.append((char*)msg.options[uriIndex].val, msg.options[uriIndex].len);
        uriIndex++;
      }
      path += "\0";
      CoapMessage msgResponse;
      Serial.printf("path: %s", path);
      this->resource.handleRequest(path.c_str(), detailCode, msg, msgResponse);*/
      size_t totalLen = 0;
      uint16_t tmpIndex = uriIndex;
      while(tmpIndex != msg.optionSize && msg.options[tmpIndex].num == 11) {
          totalLen += msg.options[tmpIndex].len;
          tmpIndex++;
      }
      uint16_t segmentCount = tmpIndex - uriIndex;
      if(segmentCount > 1) totalLen += segmentCount - 1;

      char* path = (char*)malloc(totalLen + 1);
      if(!path) return;

      path[0] = '\0';
      while(uriIndex != msg.optionSize && msg.options[uriIndex].num == 11) {
          if(strlen(path) > 0) strcat(path, "/");
          strncat(path, (char*)msg.options[uriIndex].val, msg.options[uriIndex].len);
          uriIndex++;
      }

      CoapMessage msgResponse;
      Serial.printf("path: %s\n", path);
      this->resource.handleRequest(path, detailCode, msg, msgResponse);

      free(path);
    }
  }
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

void CoapRx::addHandler(const char* path, uint8_t method, CoapHandler handler) {
  this->resource.addHandler(path, method, handler);
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

void CoapResource::handleRequest(const char* fullPath, uint8_t method, CoapMessage& req, CoapMessage& res) {
  if(!fullPath || method >= 4) return;

  char* pathCopy = (char*)malloc(strlen(fullPath)+1);
  if(!pathCopy) return;
  strcpy(pathCopy, fullPath);

  char* token = strtok(pathCopy, "/");
  CoapResource* node = this;

  while(token && node) {
    node = node->findChild(token);
    token = strtok(nullptr, "/");
  }

  if(node && node->handlers[method]) {
    node->handlers[method](req, res);
  } else {
    Serial.println("4.04 NOT_FOUND");
  }

  free(pathCopy);
}
