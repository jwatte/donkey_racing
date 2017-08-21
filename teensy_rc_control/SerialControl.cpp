#include <Arduino.h>

#include "SerialControl.h"
#include "FastCRC.h"


static FastCRC16 gKermit;


SerialControl::SerialControl(HardwareSerial &serial, bool waitForFirstPacket)
  : serial_(serial)
  , lastAutoSendTime_(0)
  , inPtr_(0)
  , waitForFirstPacket_(waitForFirstPacket)
  , lastRemoteSerial_(0)
  , mySerial_(0)
{
  memset(infos_, 0, sizeof(infos_));
  memset(datas_, 0, sizeof(datas_));
  memset(inBuf_, 0, sizeof(inBuf_));
}
    

void SerialControl::bind(uint8_t id, void *data, uint8_t size, bool autoSend = false) {
  int ix = -1;
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (infos_[i].id_ == id) {
      ix = i;
      break;
    }
    if (!datas_[i] && ix == -1) {
      ix = i;
    }
  }
  if (i == -1) {
    //  ERROR! Too many items.
    return;
  }
  datas_[ix] = data;
  infos_[ix].id = id;
  infos_[ix].flags = ((autoSend && (data != NULL)) ? FlagAutoSend : 0);
  infos_[ix].size = size;
}


void SerialControl::begin(uint32_t br) {
  serial_.begin(br);
  inPtr_ = 0;
  for (int i = 0; i != MAX_ITEMS; ++i) {
    infos_[ix].flags &= FlagAutoSend;
  }
  lastAutoSendTime_ = 0;
  mySerial_ = 0;
  lastRemoteSerial_ = 0;
}


void SerialControl::step(uint32_t now) {
  readInput(now);
  writeOutput(now);
}

void SerialControl::readInput(uint32_t now) {
  while (serial_.available()) {
    int ch = serial_.read();
    if (inPtr_ == 0 && ch != 0x55) {
      continue;
    }
    if (inPtr_ == 1 && ch != 0xAA) {
      continue;
    }
    if (inPtr_ == 4 && ch > sizeof(inBuf_)-7) {
      discardingPacket(inBuf_, inPtr_);
      //  too long packet! maybe I lost sync?
      inPtr_ = (ch == 0x55) ? 1 : 0;
      continue;
    }
    inBuf_[inPtr_] = ch;
    ++inPtr_;
    if ((inPtr_ >= 7) && (inPtr_ == (7 + inBuf_[4]))) {
      //  calculate CRC
      uint16_t crc = gKermit.kermit(&inBuf_[4], inBuf_[4] + 1);
      unsigned char *endPtr = &inBuf_[5] + inbuf_[4];
      if ((endPtr[0] == (crc & 0xff)) && (endPtr[1] == ((crc >> 8) & 0xff))) {
        lastRemoteSerial_ = inBuf_[2];
        waitingForFirstPacket_ = false;
        parseFrame(&inBuf_[5], inBuf_[4]);
      } else {
        discardingPacket(inBuf_, inPtr_);
      }
      inPtr_ = 0;
    }
  }
}

void SerialControl::writeOutput(uint32_t now) {
  if (waitingForFirstPacket_) {
    return;
  }
  if (now - lastAutoSendTime_ > 1000) {
    for (int ix = 0; ix != MAX_ITEMS; ++ix) {
      if (infos_[ix].flags & 
    }
  }
}

uint8_t SerialControl::remoteSerial() const {
  
}


bool SerialControl::isRecived(uint8_t id) const {
  
}


void SerialControl::clearRecived(uint8_t id) {
  
}


bool SerialControl::getFresh(uint8_t id) {
  
}


bool SerialControl::getFresh(void const *data) {
  
}


bool SerialControl::sendNow(uint8_t id) {
  
}


bool SerialControl::sendNow(void const *data) {
  
}


bool SerialControl::enqueuePayload(uint8_t id, void const *data, uint8_t length) {
  
}


void SerialControl::parseFrame(uint8_t const *data, uint8_t length) {
  
}


uint8_t SerialControl::parsePacket(uint8_t type, uint8_t const *data, uint8_t maxsize) {
  
}


void SerialControl::unknownPacketId(uint8_t type, uint8_t const *data, uint8_t maxsize) {
  
}

void SerialControl::discardingData(uint8_t const *base, uint8_t length) {
  
}

