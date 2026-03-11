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

void Coap::addHandler(const char* path, uint8_t method, CoapHandler handler) {
  this->resource.addHandler(path, method, handler);
}

void Coap::handleReceivedMsg() {
  CoapMessage msg; CoapTransactionContext reqContext;
  bool isNotEmpty = this->_rx.shiftMessage(msg, reqContext);
  if(!isNotEmpty) return;
  CoapTransactionContext respContext = this->resource.handleRequest(msg, reqContext);
  this->_tx.sendResponse(respContext);
}
