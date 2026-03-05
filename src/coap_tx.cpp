#include "coap.h"
void CoapTx::insertArrayToBuffer(uint16_t &iBuffer, uint8_t *entry, uint16_t entryLen) {
  if (entryLen == 0) return;
  for(uint16_t iEntry = 0; iEntry < entryLen && iBuffer < this->_maxBufferSize; iEntry++) {
    buffer[iBuffer++] = entry[iEntry];
  }
}

uint8_t CoapTx::encodeOptionField(uint16_t value, uint8_t out[3]) {
  if (value < 13) {
    out[0] = value;
    return 1;
  } 
  if (value < 269) {
    out[0] = 13;
    out[1] = value - 13;
    return 2;
  }

  out[0] = 14;
  value -= 269;
  out[1] = (value >> 8) & 0xFF;
  out[2] = value & 0xFF;
  return 3;
}

uint16_t CoapTx::setBuffer() {
  if (this->_maxBufferSize < 4) return 0;

  uint8_t tokenLen = message.tokenLen;
  if (tokenLen > 8) return 0;

  uint16_t iBuffer = 0;

  buffer[iBuffer++] =
    (message.coapVersion << 6) |
    ((message.type & 0x03) << 4) |
    (tokenLen & 0x0F);

  buffer[iBuffer++] = message.code;
  buffer[iBuffer++] = (message.messageId >> 8) & 0xFF;
  buffer[iBuffer++] = message.messageId & 0xFF;

  this->insertArrayToBuffer(iBuffer, message.token, tokenLen);

  for(uint8_t iOption = 1; iOption < message.optionSize; iOption++) {
    uint16_t deltaTemp = message.options[iOption].num - message.options[iOption - 1].num;
    uint8_t delta[3] = {0, 0, 0},  len[3] = {0, 0, 0};
    int deltaFieldSize = this->encodeOptionField(deltaTemp, delta);
    int lenFieldSize = this->encodeOptionField(message.options[iOption].len, len);

    buffer[iBuffer++] = delta[0] | len[0];
    for(uint8_t i = 1; i < deltaFieldSize; i++) buffer[iBuffer++] = delta[i];
    for(uint8_t i = 1; i < lenFieldSize; i++) buffer[iBuffer++] = len[i];
    this->insertArrayToBuffer(iBuffer, message.options[iOption].val, message.options[iOption].len);
  }

  buffer[iBuffer++] = 0xFF;

  if (message.payloadLen > 0) {
    if (iBuffer + 1 >= this->_maxBufferSize) return 0;
    this->insertArrayToBuffer(iBuffer, message.payload, message.payloadLen);
  }

  return iBuffer;
}

void CoapTx::transmitPacket(const char *ip, int port, uint16_t actualBufferSize) {
  yield();
  int ok = this->_udp->beginPacket(ip, port);
  if (!ok) {
    Serial.println("beginPacket failed");
    return;
  }
  this->_udp->write(buffer, actualBufferSize);
  this->_udp->endPacket();
  yield();

  Serial.print("CoAP Packet (");
  Serial.print(actualBufferSize);
  Serial.println(" bytes):");

  for (uint16_t i = 0; i < actualBufferSize; i++) {
    Serial.printf("%02X", buffer[i]);
    Serial.print(((i + 1) % 4 == 0) ? "\n": " ");
  }
  Serial.println();
}

void CoapTx::initMessage(uint8_t tokenLen) {
  if(tokenLen > 8) return;
  message.coapVersion = 1;
  message.type = 0;
  
  message.code = 0;
  message.messageId = random(0, 65536);
  message.payloadLen = 0;
  message.tokenLen = tokenLen;
}

void CoapTx::setMessage() {
  // regenerate token
  if(message.tokenLen > 8) return;
  for(uint8_t i = 0; i < message.tokenLen; i++) message.token[i] = random(0, 256);

  // wrap messageId
  message.messageId++;
}

void CoapTx::transmitMessage(const char *ip, int port) {
  uint16_t actualBufferSize = this->setBuffer();
  this->transmitPacket(ip, port, actualBufferSize);
  /*if(message.type == 0x00) {
    bool isReceivedAck = false;
    int retransmitionCounter = 0;
    int timeout = random(ACK_TIMEOUT, ACK_TIMEOUT * ACK_RANDOM_FACTOR);
    int startListen = millis();
    
    while (!isReceivedAck && millis() < startListen + timeout && retransmitionCounter < MAX_RETRANSMIT) {
      isReceivedAck = this->receiveMessage(CoapTx::isAckMessage, message.messageId);

      this->transmitPacket(message, ip, port);
      retransmitionCounter++;
    }
  }*/
}
