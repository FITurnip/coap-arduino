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
  CoapMessage reqMsg, respMsg;
  CoapTransactionContext reqContext, respContext;

  bool isNotEmpty = this->_rx.shiftMessage(reqMsg, reqContext);
  if(!isNotEmpty) return;

  respMsg = this->resource.handleRequest(reqMsg);
  this->_tx.setBuffer(respMsg, respContext.buffer);
  respContext.ipRemote = respMsg.ipRemote;
  reqContext.portRemote = respMsg.portRemote;
  this->_tx.sendResponse(respContext);
}
