#include "coap.h"
Coap::Coap(int port) : _tx(port), _rx(port) {
}

void Coap::start(WifiCfg wifiCfg) {
  this->_tx.start(wifiCfg);
  // attach to rx
  WiFiUDP* udp = this->_tx.getTransport();
  this->_rx.setTransport(udp);
}

CoapTx& Coap::tx() { return this->_tx; }
CoapRx& Coap::rx() { return this->_rx; }

/*
void Coap::handleBulkMessage() {
  while(!bufferQueue.empty()) {
    CoapPacket packet = bufferQueue.front();
    bufferQueue.pop();

    CoapMessage msg;
    this->parseReceived(msg, packet.data, packet.size);
    msg.diagnostic();

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
*/

void Coap::addHandler(const char* path, uint8_t method, CoapHandler handler) {
  this->_resource.addHandler(path, method, handler);
}
