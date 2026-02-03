#include "coap.h"

Coap::Coap(UDP &udp, int port, int bufferSize)
    : _udp(&udp), _port(port), bufferSize(bufferSize), receivedMessageQueue(COAP_MSG_QUEUE_SIZE) { }

Coap::~Coap() { }

void Coap::beginUdp() {
    this->_udp->begin(this->_port);
}

void CoapUri::add(String uri, void (*callback)(CoapMessage &msg)) {
  if(this->counter < MAX_URI) return;
  this->uri[counter] = uri;
  this->handler[counter++] = callback;
}

void CoapUri::handle(String uri, CoapMessage &msg) {
  bool found = false;
  int indexUri = 0;
  while(!found && indexUri < this->counter) {
    found = (uri == this->uri[indexUri++]);
  }
  if(found) this->handler[indexUri](msg);
}
