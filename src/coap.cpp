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
  this->_rx.resource.addHandler(path, method, handler);
}
