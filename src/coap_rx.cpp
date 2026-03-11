#include "coap.h"
uint16_t parseExtended(uint16_t initialVal, uint8_t* &cursor, uint8_t* endPtr) {
    if (initialVal < 13) {
        return initialVal; // Bisa bernilai 0 s/d 12
    } else if (initialVal == 13) {
        if (cursor >= endPtr) return -1; // Error: data habis
        return 13 + *cursor++;
    } else if (initialVal == 14) {
        if (cursor + 1 >= endPtr) return -1; // Error: data tidak cukup 2 byte
        uint16_t val = 269 + ((uint16_t)cursor[0] << 8 | cursor[1]);
        cursor += 2;
        return val;
    }
    
    // Jika initialVal == 15, ini "Reserved" menurut RFC 7252 (Invalid)
    return -1; 
}

uint16_t decode16Bit(uint8_t *ptr) {
  return ((uint16_t)ptr[0] << 8) | ptr[1];
}

void CoapRx::parseReceived(CoapMessage &msg, CoapBuffer &buffer) {
  if(buffer.size < 4) return; // Header minimal 4 byte
  
  uint8_t *readPtr = buffer.data;
  uint8_t *endPtr = buffer.data + buffer.size;

  // --- Header ---
  msg.coapVersion = (*readPtr) >> 6;
  msg.type        = (CoapType)((*readPtr >> 4) & 0x03);
  msg.token.size  = (*readPtr++) & 0x0F;
  msg.code        = *readPtr++;
  msg.messageId   = decode16Bit(readPtr);
  readPtr += 2;

  // --- Token ---
  if (msg.token.size > 8) return; 
  if (msg.token.size > 0) {
    if (readPtr + msg.token.size > endPtr) return; // Boundary check
    memcpy(msg.token.data, readPtr, msg.token.size);
    readPtr += msg.token.size;
  }

  // --- Options ---
  uint16_t prevOptionNum = 0;
  while (readPtr < endPtr) {
    if (*readPtr == 0xFF) break; // Payload marker ditemukan

    uint8_t optField = *readPtr++;
    uint16_t optDelta = parseExtended((optField >> 4) & 0x0F, readPtr, endPtr);
    uint16_t optLen   = parseExtended(optField & 0x0F, readPtr, endPtr);
    prevOptionNum += optDelta;
    if (readPtr + optLen > endPtr) return; 
    msg.addOption((CoapOptNum)prevOptionNum, optLen, readPtr);
    readPtr += optLen;
  }

  // --- Payload ---
  if (readPtr < endPtr && *readPtr == 0xFF) {
    readPtr++;
    
    uint16_t payloadSize = endPtr - readPtr;
    if (payloadSize > 0) {
      if (payloadSize > DEFAULT_PAYLOAD_SIZE) {
         msg.payload.size = 0; // Atau handle error overflow
      } else {
        msg.payload.size = payloadSize;
        memcpy(msg.payload.data, readPtr, payloadSize);
      }
    } else {
      msg.payload.size = 0; // Marker ada tapi payload kosong (RFC melarang ini tapi kita handle safe)
    }
  } else {
    msg.payload.size = 0; // Tidak ada payload
  }
}

bool CoapRx::receiveMessage() {
  CoapTransactionContext transactionContext;
  bool ok = this->_receive(transactionContext);
  if (ok) {
    transactionQueue.push(transactionContext);
  }
  return ok;
}

bool CoapRx::shiftMessage(CoapMessage &msg, CoapTransactionContext &transactionContext) {
  if(transactionQueue.isEmpty()) return false;
  transactionContext = transactionQueue.front();
  transactionQueue.pop();
  
  this->parseReceived(msg, transactionContext.buffer);
  msg.ipRemote = transactionContext.ipRemote;
  msg.portRemote = transactionContext.portRemote;
  msg.print();
  return true;
}

