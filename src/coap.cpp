#include "coap.h"

CoapBase::CoapBase(UDP &udp, int port, int maxBufferSize) : _udp(&udp), _port(port), _maxBufferSize(maxBufferSize) {
  buffer = (uint8_t*) malloc(this->_maxBufferSize);
}


void CoapBase::beginConnection() {
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
