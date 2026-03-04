#include "coap.h"
void Coap::parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen)
{
  // --- Header ---
  msg.coapVersion   = buffer[0] >> 6;
  msg.type          = (buffer[0] >> 4) & 0x03;
  msg.tokenLen      = buffer[0] & 0x0F;
  msg.code          = buffer[1];
  msg.messageId     = (buffer[2] << 8) | buffer[3];

  if (msg.tokenLen > 8) return;

  uint8_t iBuffer = 4;

  // --- Token ---
  if (msg.tokenLen > 0) {
    //msg.token = (uint8_t*) malloc(msg.tokenLen);
    memcpy(msg.token, &buffer[iBuffer], msg.tokenLen);
  }

  // --- Options (skip) ---
  /*while (iBuffer < bufferLen && buffer[iBuffer] != 0xFF) {
    uint8_t opt     = buffer[iBuffer];
    uint8_t optLen  = opt & 0x0F;
    iBuffer         += 1 + optLen;
  }*/

  // --- Payload ---
  /*if (iBuffer < bufferLen && buffer[iBuffer] == 0xFF) {
    iBuffer++;
    msg.payload     = &buffer[iBuffer];
    msg.payloadLen  = bufferLen - iBuffer;
  } else {
    msg.payload     = nullptr;
    msg.payloadLen  = 0;
  }*/

  Serial.println("-------------------------------");
  Serial.printf("Vers\t:\t%d\n", msg.coapVersion);
  Serial.printf("Type\t:\t%d\n", msg.type);
  Serial.printf("TKL\t:\t%d\n", msg.tokenLen);
  Serial.printf("Code\t:\t%d\n", msg.code);
  Serial.printf("Msg Id\t:\t%d\n", msg.messageId);
  if(msg.tokenLen > 0) {
    Serial.print("Token\t:\t");
    for(uint8_t i = 0; i < msg.tokenLen; i++) Serial.printf("%02X ", (unsigned int)msg.token[i]);
    Serial.print("\n");
  }
  Serial.println("-------------------------------");
}


bool Coap::receiveMessage(PacketFilter filter, uint16_t matchMsgId) {
  int packetLen = this->_udp->parsePacket();
  if (packetLen == 0) return false;

  uint8_t buffer[DEFAULT_BUFFER_SIZE];
  int len = this->_udp->read(buffer, sizeof(buffer));
  if (len < 4) return false;

  if (filter && !filter(buffer, len, matchMsgId)) return false;

  receivedMessageQueue.enqueue(buffer, len);
  return true;
}

bool Coap::isAckMessage(const uint8_t* data, int len, uint16_t matchMsgId) {
    uint8_t type    = (data[0] >> 4) & 0x03;
    uint16_t msgId  = (data[2] << 8) | data[3];

    return (type == 2) && (msgId == matchMsgId);
}

void Coap::handleBulkMessage() {
  while(!this->receivedMessageQueue.isEmpty()) {
    uint8_t buffer[DEFAULT_BUFFER_SIZE];
    int bufferLen = DEFAULT_BUFFER_SIZE;
    this->receivedMessageQueue.dequeue(buffer, bufferLen);

    // handle
    CoapMessage msg;
    this->parseReceived(msg, buffer, bufferLen);
    //this->uri.handle("", msg);
    delay(100);
  }
}
