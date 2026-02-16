#include "coap.h"
void Coap::insertArrayToBuffer(uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen) {
  if (entryLen == 0) return;
  for(uint16_t iEntry = 0; iEntry < entryLen && iBuffer < this->bufferSize; iEntry++) {
    transmittedMessage[iBuffer++] = entry[iEntry];
  }
}

uint16_t Coap::setBuffer(CoapMessage &msg) {
  if (this->bufferSize < 4) return 0;

  uint8_t tokenLen = msg.tokenLen;
  if (tokenLen > 8) return 0;

  uint16_t iBuffer = 0;

  transmittedMessage[iBuffer++] =
    (msg.coapVersion << 6) |
    ((msg.type & 0x03) << 4) |
    (tokenLen & 0x0F);

  transmittedMessage[iBuffer++] = msg.code;
  transmittedMessage[iBuffer++] = (msg.messageId >> 8) & 0xFF;
  transmittedMessage[iBuffer++] = msg.messageId & 0xFF;

  this->insertArrayToBuffer(iBuffer, msg.token, tokenLen);

  if (msg.payloadLen > 0) {
    if (iBuffer + 1 >= this->bufferSize) return 0;
    transmittedMessage[iBuffer++] = 0xFF;
    this->insertArrayToBuffer(iBuffer, msg.payload, msg.payloadLen);
  }

  return iBuffer;
}
/*
uint16_t Coap::setBuffer(CoapMessage &msg) {
  //if (transmittedMessage != NULL) {
    free(transmittedMessage);
    transmittedMessage = NULL;
  }
  transmittedMessage = (uint8_t *) malloc(this->bufferSize);
  uint16_t iBuffer = 0;
  transmittedMessage[iBuffer++] =
    (msg.coapVersion << 6) |
    ((msg.type & 0x03) << 4) |
    (msg.tokenLen & 0x0F);
  
  //transmittedMessage[0] = msg.coapVersion << 6;
  //transmittedMessage[0] |= (msg.type & 0x03) << 4;
  //transmittedMessage[0] |= (msg.tokenLen & 0x0F);

  transmittedMessage[iBuffer++] = msg.code;
  transmittedMessage[iBuffer++] = (msg.messageId >> 8) & 0xFF;
  transmittedMessage[iBuffer++] = msg.messageId & 0xFF;

  // here, future for generating token
  this->insertArrayToBuffer(iBuffer, msg.token, msg.tokenLen);
  // here, future for inserting options
  
  if (msg.payloadLen > 0) {
    transmittedMessage[iBuffer++] = 0xFF;
    this->insertArrayToBuffer(iBuffer, msg.payload, msg.payloadLen);
  }

  return iBuffer;
}*/

void Coap::transmitUdpPacket(CoapMessage &msg, uint16_t bufferLen, const char *ip, int port) {
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

void Coap::transmitMessage(CoapMessage &msg, const char *ip, int port) {
  uint16_t bufferLen = this->setBuffer(msg);
  this->transmitUdpPacket(msg, bufferLen, ip, port);
  /*if(msg.type == 0x00) {
    bool isReceivedAck = false;
    int retransmitionCounter = 0;
    int timeout = random(ACK_TIMEOUT, ACK_TIMEOUT * ACK_RANDOM_FACTOR);
    int startListen = millis();
    
    while (!isReceivedAck && millis() < startListen + timeout && retransmitionCounter < MAX_RETRANSMIT) {
      isReceivedAck = this->receiveMessage(Coap::isAckMessage, msg.messageId);

      this->transmitUdpPacket(msg, ip, port);
      retransmitionCounter++;
    }
  }*/
}
