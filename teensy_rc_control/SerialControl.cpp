
#include "SerialControl.h"
#include "FastCRC.h"
#include "Support.h"

#include <string.h>
#include <HardwareSerial.h>


static FastCRC16 gKermit;


SerialControl::SerialControl(HardwareSerial &serial, bool waitForFirstPacket)
  : serial_(serial)
  , lastAutoSendTime_(0)
  , inPtr_(0)
  , outPtr_(0)
  , waitForFirstPacket_(waitForFirstPacket)
  , lastRemoteSerial_(0)
  , mySerial_(0)
{
  memset(infos_, 0, sizeof(infos_));
  memset(datas_, 0, sizeof(datas_));
  memset(inBuf_, 0, sizeof(inBuf_));
  memset(outBuf_, 0, sizeof(outBuf_));
}
    

void SerialControl::bind(uint8_t id, void *data, uint8_t size, bool autoSend) {
  int ix = -1;
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (infos_[i].id == id) {
      ix = i;
      break;
    }
    if (!datas_[i] && ix == -1) {
      ix = i;
    }
  }
  if (ix == -1) {
    CRASH_ERROR("Too Many SerialControl Items");
  }
  datas_[ix] = data;
  infos_[ix].id = id;
  infos_[ix].flags = ((autoSend && (data != NULL)) ? FlagAutoSend : 0);
  infos_[ix].size = size;
}


void SerialControl::begin(uint32_t br) {
  serial_.begin(br);
  inPtr_ = 0;
  outPtr_ = 0;
  for (int i = 0; i != MAX_ITEMS; ++i) {
    infos_[i].flags &= FlagAutoSend;
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
    if ((inPtr_ == 4) && (ch > (int)sizeof(inBuf_) - 7)) {
      discardingData(inBuf_, inPtr_);
      //  too long packet! maybe I lost sync?
      inPtr_ = (ch == 0x55) ? 1 : 0;
      continue;
    }

    inBuf_[inPtr_] = ch;
    ++inPtr_;

    if ((inPtr_ >= 7) && (inPtr_ == (7 + inBuf_[4]))) {
      //  calculate CRC
      uint16_t crc = gKermit.kermit(&inBuf_[4], inBuf_[4] + 1);
      unsigned char *endPtr = &inBuf_[5] + inBuf_[4];
      if ((endPtr[0] == (crc & 0xff)) && (endPtr[1] == ((crc >> 8) & 0xff))) {
        lastRemoteSerial_ = inBuf_[2];
        waitForFirstPacket_ = false;
        parseFrame(&inBuf_[5], inBuf_[4]);
      } else {
        discardingData(inBuf_, inPtr_);
      }
      inPtr_ = 0;
    }

    if (inPtr_ == sizeof(inBuf_)) {
      CRASH_ERROR("Overrun SerialControl inBuf_");
    }
  }
}


void SerialControl::writeOutput(uint32_t now) {

  /* Send nothing until the host has connected */
  if (waitForFirstPacket_) {
    return;
  }

  bool failedToAutoSend = false;
  if (now - lastAutoSendTime_ >= 1000) {

    /* Look for auto-send packets that haven't been sent yet. */
    for (int ix = 0; ix != MAX_ITEMS; ++ix) {
      if ((infos_[ix].flags & (FlagAutoSend | FlagWasAutoSent)) == FlagAutoSend) {
        if (enqueuePayload(infos_[ix].id, datas_[ix], infos_[ix].size)) {
          infos_[ix].flags |= FlagWasAutoSent;
        } else {
          /*  Note that we're waiting to send something but keep looking 
           *  for perhaps smaller packets that could still fit.
           */
          failedToAutoSend = true;
        }
      }
    }

    if (!failedToAutoSend) {
      /* OK, nothing more to do until next time. */
      lastAutoSendTime_ = now;
      /* Make sure packets are considered for sending next time around. */
      for (int ix = 0; ix != MAX_ITEMS; ++ix) {
        infos_[ix].flags &= ~FlagWasAutoSent;
      }
    }
  }

  if (!failedToAutoSend) {
    /*  If I'm not in a situation where I'm waiting for the send queue 
     *  to drain to fulfill my auto-send obligations, start stuffing 
     *  payload data into the channel.
     */
    for (int ix = 0; ix != MAX_ITEMS; ++ix) {
      if (infos_[ix].flags & FlagToSend) {
        if (enqueuePayload(infos_[ix].id, datas_[ix], infos_[ix].size)) {
          infos_[ix].flags &= ~FlagToSend;
        }
      }
    }
  }

  /* if there's data to be written, attempt to form a packet and send it */
  if (outPtr_ > 0) {
    /* only write a packet if the serial port is ready */
    if (serial_.availableForWrite() >= outPtr_ + 2) {
      if (outPtr_ > sizeof(outBuf_) - 2) {
        CRASH_ERROR("Overrun SerialCommand ouBuf_");
      }
      uint16_t crc = gKermit.kermit(&outBuf_[4], outPtr_-4);
      outBuf_[outPtr_++] = crc & 0xff;
      outBuf_[outPtr_++] = (crc >> 8) & 0xff;
      serial_.write(outBuf_, outPtr_);
      outPtr_ = 0;
    }
  }
}

uint8_t SerialControl::remoteSerial() const {
  return lastRemoteSerial_;
}


bool SerialControl::isRecived(uint8_t id) const {
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (infos_[i].id == id) {
      return (infos_[i].flags & FlagReceived) == FlagReceived;
    }
  }
  return false;
}


void SerialControl::clearRecived(uint8_t id) {
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (infos_[i].id == id) {
      infos_[i].flags &= ~FlagReceived;
      return;
    }
  }
}


bool SerialControl::getFresh(uint8_t id) {
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (infos_[i].id == id) {
      bool ret = (infos_[i].flags & FlagReceived) == FlagReceived;
      infos_[i].flags &= ~FlagReceived;
      return ret;
    }
  }
  return false;
}


bool SerialControl::getFresh(void const *data) {
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (datas_[i] == data) {
      bool ret = (infos_[i].flags & FlagReceived) == FlagReceived;
      infos_[i].flags &= ~FlagReceived;
      return ret;
    }
  }
  return false;
}


bool SerialControl::sendNow(uint8_t id) {
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (infos_[i].id == id) {
      if (enqueuePayload(id, datas_[i], infos_[i].size)) {
        return true;
      }
      infos_[i].flags |= FlagToSend;
      break;
    }
  }
  return false;
}


bool SerialControl::sendNow(void const *data) {
  for (int i = 0; i != MAX_ITEMS; ++i) {
    if (datas_[i] == data) {
      if (enqueuePayload(infos_[i].id, datas_[i], infos_[i].size)) {
        return true;
      }
      infos_[i].flags |= FlagToSend;
      break;
    }
  }
  return false;
}


bool SerialControl::enqueuePayload(uint8_t id, void const *data, uint8_t length) {
  uint8_t needed = length + 2;
  if (outPtr_ == 0) {
    needed += 5;
  }
  if (needed > sizeof(outBuf_) - outPtr_) {
    return false;
  }
  if (outPtr_ == 0) {
    ++mySerial_;
    if (mySerial_ == 0) {
      mySerial_ = 1;
    }
    outPtr_ = 5;
  }
  outBuf_[0] = 0x55;
  outBuf_[1] = 0xAA;
  outBuf_[2] = mySerial_;
  outBuf_[3] = lastRemoteSerial_;
  outBuf_[4] = outPtr_ + length - 5;
  memmove(&outBuf_[outPtr_], data, length);
  outPtr_ += length;
}


void SerialControl::parseFrame(uint8_t const *data, uint8_t length) {
  while (length > 0) {
    uint8_t id = *data;
    for (int i = 0; i != MAX_ITEMS; ++i) {
      if (infos_[i].id == id) {
        
      }
    }
  }
}


uint8_t SerialControl::parsePacket(uint8_t type, uint8_t const *data, uint8_t maxsize) {
  
}


void SerialControl::unknownPacketId(uint8_t type, uint8_t const *data, uint8_t maxsize) {
  //  do nothing
}

void SerialControl::discardingData(uint8_t const *base, uint8_t length) {
  //  do nothing
}

