#include "coap.h"
uint16_t parseExtended(uint16_t initialVal, uint8_t* &cursor, uint8_t* endPtr) {
    if (initialVal < 13) {
        return initialVal; // Bisa bernilai 0 s/d 12
    } else if (initialVal == 13) {
        if (cursor >= endPtr) return -1; // Error: data habis
        return 13 + *cursor++;
    } else if (initialVal == 14) {
        if (cursor + 1 >= endPtr) return -1; // Error: data tidak cukup 2 byte
        uint16_t val = 269 + ((uint16_t)cursor[0] << 8 | cursor[1]);
        cursor += 2;
        return val;
    }
    
    // Jika initialVal == 15, ini "Reserved" menurut RFC 7252 (Invalid)
    return -1; 
}

uint16_t decode16Bit(uint8_t *ptr) {
  return ((uint16_t)ptr[0] << 8) | ptr[1];
}

void CoapRx::parseReceived(CoapMessage &msg, CoapBuffer &buffer) {
  if(buffer.size > DEFAULT_BUFFER_SIZE) return;
  // --- Header ---
  uint8_t *readPtr = buffer.data, *endPtr = buffer.data + buffer.size;;
  msg.coapVersion   = *readPtr >> 6;
  msg.type          = (CoapType) ((*readPtr >> 4) & 0x03);
  msg.token.size    = *readPtr++ & 0x0F;
  msg.code          = *readPtr++;
  msg.messageId     = decode16Bit(readPtr);
  readPtr += 2;

  // --- Token ---
  if (msg.token.size > 8) return;
  if (msg.token.size > 0) {
    memcpy(msg.token.data, readPtr, msg.token.size);
    readPtr += msg.token.size;
  }

  // --- Options ---
  uint16_t prevOptionNum = 0;
  while (readPtr < endPtr) {
    if(*readPtr == 0xFF) break;

    uint8_t   optField  = *readPtr++;
    uint16_t  optDelta  = parseExtended((optField >> 4) & 0x0F, readPtr, endPtr);
    uint16_t  optLen    = parseExtended(optField & 0x0F, readPtr, endPtr);
    if (optDelta < 0 || optLen < 0) break;

    CoapOptNum optionNum = (CoapOptNum) (prevOptionNum + optDelta);
    prevOptionNum = optionNum;

    if(readPtr + optLen > endPtr) break;
    msg.addOption(optionNum, optLen, readPtr);
    readPtr+=optLen;
  }

  // --- Payload ---
  if(readPtr < endPtr && *readPtr == 0xFF) readPtr++;
  msg.payload.size = readPtr < endPtr ? endPtr - readPtr : 0;
  if(msg.payload.size > DEFAULT_PAYLOAD_SIZE) msg.payload.size = 0;
  else if (msg.payload.size > 0) memcpy(msg.payload.data, readPtr, msg.payload.size);
}

bool CoapRx::receiveMessage() {
  CoapTransactionContext transactionContext;
  bool ok = this->_receive(transactionContext);
  if (ok) {
    transactionQueue.push(transactionContext);
  }
  return ok;
}

bool CoapRx::shiftMessage(CoapMessage &msg, CoapTransactionContext &transactionContext) {
  if(transactionQueue.isEmpty()) return false;
  transactionContext = transactionQueue.front();
  transactionQueue.pop();
  this->parseReceived(msg, transactionContext.buffer);
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

CoapTransactionContext CoapResource::handleRequest(CoapMessage& msg, CoapTransactionContext &reqContext) {
  CoapTransactionContext respContext = {};
  respContext.type = (reqContext.type == COAP_CON) ? COAP_ACK : COAP_NON;
  respContext.messageId = msg.messageId;
  respContext.token = msg.token;
  uint8_t clsCode = msg.code >> 5, detailCode = msg.code & 0x1F;

  if(!(clsCode == 0 && detailCode > 0 && detailCode < 4)) {
    respContext.code = 163;
    return respContext;
  }

  CoapOpt *readUriOpt = msg.options, *endOpt = msg.options + msg.optionSize;
  while(readUriOpt < endOpt && readUriOpt->num != URI_PATH) readUriOpt++;

  CoapOpt *endUriOpt = readUriOpt;
  while(endUriOpt < endOpt && endUriOpt->num == URI_PATH) endUriOpt++;

  CoapResource* node = this;
  char pathBuf[32];
  while(node && readUriOpt < endUriOpt) {
    uint8_t len = (readUriOpt->len > 31) ? 31 : readUriOpt->len;
    memcpy(pathBuf, readUriOpt->val, len);
    pathBuf[len] = '\0';
    node = node->findChild(pathBuf);
    readUriOpt++;
  }

  if(!node) {
    respContext.code = 132;
    return respContext;
  }

  if(node->handlers[detailCode]) {
    respContext = node->handlers[detailCode](msg);
    return respContext;
  }

  respContext.code = 133;
  return respContext;
}
