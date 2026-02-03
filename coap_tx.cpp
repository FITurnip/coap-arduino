#include "coap.h"
void Coap::insertArrayToBuffer(uint8_t &iBuffer, uint8_t *entry, uint8_t entryLen) {
  if (entryLen == 0) return;
  for(uint8_t iEntry = 0; iEntry < entryLen; iEntry++, iBuffer++) {
    transmittedMessage[iBuffer] = entry[iEntry];
  }
}

void Coap::setBuffer(CoapMessage &msg) {
  if (transmittedMessage != NULL) {
    free(transmittedMessage);
    transmittedMessage = NULL;
  }
  transmittedMessage = (uint8_t *) malloc(this->bufferSize);
  
  transmittedMessage[0] = msg.coapVersion << 6;
  transmittedMessage[0] |= (msg.type & 0x03) << 4;
  transmittedMessage[0] |= (msg.tokenLen & 0x0F);

  transmittedMessage[1] = msg.code;
  transmittedMessage[2] = (msg.messageId >> 8) & 0xFF;
  transmittedMessage[3] = msg.messageId & 0xFF;

  uint8_t iBuffer = 4;
  // here, future for generating token
  this->insertArrayToBuffer(iBuffer, msg.token, msg.tokenLen);
  // here, future for inserting options
  
  transmittedMessage[iBuffer++] = 0xFF;
  this->insertArrayToBuffer(iBuffer, msg.payload, msg.payloadLen);
}

void Coap::transmitUdpPacket(CoapMessage &msg, const char *ip, int port) {
  this->_udp->beginPacket(ip, port);
  this->_udp->write(transmittedMessage, msg.payloadLen);
  this->_udp->endPacket();
}

void Coap::transmitMessage(CoapMessage &msg, const char *ip, int port) {
  this->setBuffer(msg);
  this->transmitUdpPacket(msg, ip, port);
  if(msg.type == 0x00) {
    bool isReceivedAck = false;
    int retransmitionCounter = 0;
    int timeout = random(ACK_TIMEOUT, ACK_TIMEOUT * ACK_RANDOM_FACTOR);
    int startListen = millis();
    
    while (!isReceivedAck && millis() < startListen + timeout && retransmitionCounter < MAX_RETRANSMIT) {
      isReceivedAck = this->receiveMessage(Coap::isAckMessage, msg.messageId);

      this->transmitUdpPacket(msg, ip, port);
      retransmitionCounter++;
    }
  }
}
