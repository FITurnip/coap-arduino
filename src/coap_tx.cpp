#include "coap.h"
void Coap::insertArrayToBuffer(uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen) {
  if (entryLen == 0) return;
  for(uint16_t iEntry = 0; iEntry < entryLen && iBuffer < this->bufferSize; iEntry++) {
    transmittedMessage[iBuffer++] = entry[iEntry];
  }
}

uint16_t Coap::setBuffer() {
  if (this->bufferSize < 4) return 0;

  uint8_t tokenLen = coapMessage.tokenLen;
  if (tokenLen > 8) return 0;

  uint16_t iBuffer = 0;

  transmittedMessage[iBuffer++] =
    (coapMessage.coapVersion << 6) |
    ((coapMessage.type & 0x03) << 4) |
    (tokenLen & 0x0F);

  transmittedMessage[iBuffer++] = coapMessage.code;
  transmittedMessage[iBuffer++] = (coapMessage.messageId >> 8) & 0xFF;
  transmittedMessage[iBuffer++] = coapMessage.messageId & 0xFF;

  this->insertArrayToBuffer(iBuffer, coapMessage.token, tokenLen);

  if (coapMessage.payloadLen > 0) {
    if (iBuffer + 1 >= this->bufferSize) return 0;
    transmittedMessage[iBuffer++] = 0xFF;
    this->insertArrayToBuffer(iBuffer, coapMessage.payload, coapMessage.payloadLen);
  }

  return iBuffer;
}
/*
uint16_t Coap::setBuffer(CoapMessage &coapMessage) {
  //if (transmittedMessage != NULL) {
    free(transmittedMessage);
    transmittedMessage = NULL;
  }
  transmittedMessage = (uint8_t *) malloc(this->bufferSize);
  uint16_t iBuffer = 0;
  transmittedMessage[iBuffer++] =
    (coapMessage.coapVersion << 6) |
    ((coapMessage.type & 0x03) << 4) |
    (coapMessage.tokenLen & 0x0F);
  
  //transmittedMessage[0] = coapMessage.coapVersion << 6;
  //transmittedMessage[0] |= (coapMessage.type & 0x03) << 4;
  //transmittedMessage[0] |= (coapMessage.tokenLen & 0x0F);

  transmittedMessage[iBuffer++] = coapMessage.code;
  transmittedMessage[iBuffer++] = (coapMessage.messageId >> 8) & 0xFF;
  transmittedMessage[iBuffer++] = coapMessage.messageId & 0xFF;

  // here, future for generating token
  this->insertArrayToBuffer(iBuffer, coapMessage.token, coapMessage.tokenLen);
  // here, future for inserting options
  
  if (coapMessage.payloadLen > 0) {
    transmittedMessage[iBuffer++] = 0xFF;
    this->insertArrayToBuffer(iBuffer, coapMessage.payload, coapMessage.payloadLen);
  }

  return iBuffer;
}*/

void Coap::transmitUdpPacket(CoapMessage &coapMessage, uint16_t bufferLen, const char *ip, int port) {
  yield();
  int ok = this->_udp->beginPacket(ip, port);
  if (!ok) {
    Serial.println("beginPacket failed");
    return;
  }
  this->_udp->write(transmittedMessage, bufferLen);
  this->_udp->endPacket();
  yield();

  Serial.print("CoAP Packet (");
  Serial.print(bufferLen);
  Serial.println(" bytes):");

  for (uint16_t i = 0; i < bufferLen; i++) {
    if (transmittedMessage[i] < 0x10) Serial.print("0");
    Serial.print(transmittedMessage[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void Coap::initMessage(uint8_t tokenLen) {
  if(tokenLen > 8) {
    return;
  }
  coapMessage.coapVersion = 1;
  coapMessage.type = 0;
  
  coapMessage.code = 0;
  coapMessage.messageId = random(0, 65536);
  coapMessage.payloadLen = 0;
  coapMessage.tokenLen = tokenLen;
}

void Coap::setMessage() {
  // regenerate token
  if(maxTokenLen > 8) {
    return;
  }
  for(uint8_t i = 0; i < coapMessage.tokenLen; i++) coapMessage.token[i] = random(0, 256);

  // wrap messageId
  coapMessage.messageId++;
}

void Coap::transmitMessage(const char *ip, int port) {
  uint16_t bufferLen = this->setBuffer();
  this->transmitUdpPacket(coapMessage, bufferLen, ip, port);
  /*if(coapMessage.type == 0x00) {
    bool isReceivedAck = false;
    int retransmitionCounter = 0;
    int timeout = random(ACK_TIMEOUT, ACK_TIMEOUT * ACK_RANDOM_FACTOR);
    int startListen = millis();
    
    while (!isReceivedAck && millis() < startListen + timeout && retransmitionCounter < MAX_RETRANSMIT) {
      isReceivedAck = this->receiveMessage(Coap::isAckMessage, coapMessage.messageId);

      this->transmitUdpPacket(coapMessage, ip, port);
      retransmitionCounter++;
    }
  }*/
}
