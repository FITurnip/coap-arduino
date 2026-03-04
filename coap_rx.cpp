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
    memcpy(msg.token, &buffer[iBuffer], msg.tokenLen);
    iBuffer += msg.tokenLen;
  }

  // --- Options (skip) ---
  while (iBuffer < bufferLen && buffer[iBuffer] != 0xFF) {
    uint8_t opt     = buffer[iBuffer];
    uint8_t optLen  = opt & 0x0F;
    iBuffer         += 1 + optLen;
  }

  // --- Payload ---
  if (iBuffer < bufferLen && buffer[iBuffer] == 0xFF) {
    iBuffer++;
    msg.payload     = &buffer[iBuffer];
    msg.payloadLen  = bufferLen - iBuffer;
  } else {
    msg.payload     = nullptr;
    msg.payloadLen  = 0;
  }

  Serial.printf("version\t:\t%d\n", msg.coapVersion);
  Serial.printf("type\t:\t%d\n", msg.type);
  Serial.printf("code\t:\t%d\n", msg.code);
  Serial.printf("msg Id\t:\t%d\n", msg.messageId);
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
    //Serial.println("di sini!");
    uint8_t buffer[DEFAULT_BUFFER_SIZE];
    int bufferLen = DEFAULT_BUFFER_SIZE;
    this->receivedMessageQueue.dequeue(buffer, bufferLen);

    // handle
    CoapMessage msg;
    this->parseReceived(msg, buffer, bufferLen);
    this->uri.handle("", msg);
  }
}
