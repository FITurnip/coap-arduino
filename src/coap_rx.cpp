#include "coap.h"
CoapRx::CoapRx(UDP &udp, int port, int bufferSize)
    : CoapBase(udp, port, bufferSize), messageQueue(COAP_MSG_QUEUE_SIZE)
{
}

void CoapRx::parseReceived(CoapMessage &msg, uint8_t *buffer, int bufferLen) {
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
    uint8_t optField  = buffer[iBuffer];
    uint16_t optDelta  = buffer[iBuffer] >> 4;
    uint16_t optLen    = buffer[iBuffer] & 0x0F;

    if(optDelta == 14) optDelta = buffer[++iBuffer] | buffer[++iBuffer];
    else if(optDelta == 13) optDelta = buffer[++iBuffer];

    if(optLen == 14) optLen = buffer[++iBuffer] | buffer[++iBuffer];
    else if(optLen == 13) optLen = buffer[++iBuffer];

    msg.options[msg.optionSize].num = (msg.optionSize > 0) ? optDelta + msg.options[msg.optionSize - 1].num : optDelta;
    msg.options[msg.optionSize].len = optLen;
    msg.options[msg.optionSize++].val = (uint8_t*) malloc(optLen);
    for(uint16_t i = 0; i < optLen; i++) {
      msg.options[msg.optionSize].val[i] = buffer[iBuffer++];
    }
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
}


bool CoapRx::receiveMessage() {
  uint16_t bufferSize = this->_udp->parsePacket();
  if (bufferSize < 4) return false; // header only

  uint8_t buffer[bufferSize];
  this->_udp->read(buffer, bufferSize);
  messageQueue.enqueue(buffer, bufferSize);
  return true;
}

void CoapRx::handleBulkMessage() {
  while(!this->messageQueue.isEmpty()) {
    int actualBufferSize = _maxBufferSize; // temp
    this->messageQueue.dequeue(buffer, actualBufferSize);

    CoapMessage msg;
    this->parseReceived(msg, buffer, actualBufferSize);
    msg.diagnostic();
    //this->uri.handle("", msg);
  }
}
