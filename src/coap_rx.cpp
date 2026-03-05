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

  // --- Options ---
  uint16_t prevOptionNum = 0;
  while (iBuffer < bufferLen) {
    if(buffer[iBuffer] == 0xFF) {
      iBuffer++;
      break;
    }

    uint8_t   optField  = buffer[iBuffer++];
    uint16_t  optDelta  = (optField >> 4) & 0x0F;
    uint16_t  optLen    = optField & 0x0F;

    // extended delta
    if(optDelta == 13) {
      if(iBuffer >= bufferLen) break; // safety
      optDelta = 13 + buffer[iBuffer++];
    } else if(optDelta == 14) {
      if(iBuffer + 1 >= bufferLen) break;
      optDelta = 269 + (buffer[iBuffer] << 8 | buffer[iBuffer + 1]);
      iBuffer += 2;
    } else if(optDelta == 15) {
      // reserved, invalid
      break;
    }

    // extended length
    if(optLen == 13) {
      if(iBuffer >= bufferLen) break;
      optLen = 13 + buffer[iBuffer++];
    } else if(optLen == 14) {
      if(iBuffer + 1 >= bufferLen) break;
      optLen = 269 + (buffer[iBuffer] << 8 | buffer[iBuffer + 1]);
      iBuffer += 2;
    } else if(optLen == 15) {
      // reserved, invalid
      break;
    }

    uint16_t optionNum = prevOptionNum + optDelta;
    prevOptionNum = optionNum;

    if(iBuffer + optLen > bufferLen) break;

    uint8_t* val = (uint8_t*)malloc(optLen);
    memcpy(val, buffer + iBuffer, optLen);
    iBuffer += optLen;

    msg.options[msg.optionSize].num = optionNum;
    msg.options[msg.optionSize].len = optLen;
    msg.options[msg.optionSize].val = val;
    msg.optionSize++;
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
