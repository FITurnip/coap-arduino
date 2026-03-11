#include "coap.h"

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


CoapMessage CoapResource::handleRequest(CoapMessage& reqMsg) {
  CoapMessage respMsg;
  respMsg.type = (reqMsg.type == COAP_CON) ? COAP_ACK : COAP_NON;
  respMsg.messageId = reqMsg.messageId;
  respMsg.token = reqMsg.token;
  respMsg.ipRemote = reqMsg.ipRemote;
  respMsg.portRemote = reqMsg.portRemote;

  uint8_t clsCode = reqMsg.code >> 5, detailCode = reqMsg.code & 0x1F;

  if(!(clsCode == 0 && detailCode > 0 && detailCode < 4)) {
    respMsg.code = 163;
    return respMsg;
  }

  CoapOpt *readUriOpt = reqMsg.options, *endOpt = reqMsg.options + respMsg.optionSize;
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
    respMsg.code = 132;
    return respMsg;
  }

  if(node->handlers[detailCode]) {
    CoapRxConfig cfg = node->handlers[detailCode](reqMsg);
    if(cfg.dstIp[0] != 0) respMsg.ipRemote= cfg.dstIp;
    if(cfg.dstPort != 0) respMsg.portRemote = cfg.dstPort;
    respMsg.payload = cfg.payload;
    return respMsg;
  }

  respMsg.code = 133;
  return respMsg;
}
