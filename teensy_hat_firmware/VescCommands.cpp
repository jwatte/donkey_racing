#include "VescCommands.h"

extern "C" {
#include "crc.h"
}

// Communication commands
typedef enum {
  COMM_FW_VERSION = 0,
  COMM_JUMP_TO_BOOTLOADER,
  COMM_ERASE_NEW_APP,
  COMM_WRITE_NEW_APP_DATA,
  COMM_GET_VALUES,
  COMM_SET_DUTY,
  COMM_SET_CURRENT,
  COMM_SET_CURRENT_BRAKE,
  COMM_SET_RPM,
  COMM_SET_POS,
  COMM_SET_HANDBRAKE,
  COMM_SET_DETECT,
  COMM_SET_SERVO_POS,
  COMM_SET_MCCONF,
  COMM_GET_MCCONF,
  COMM_GET_MCCONF_DEFAULT,
  COMM_SET_APPCONF,
  COMM_GET_APPCONF,
  COMM_GET_APPCONF_DEFAULT,
  COMM_SAMPLE_PRINT,
  COMM_TERMINAL_CMD,
  COMM_PRINT,
  COMM_ROTOR_POSITION,
  COMM_EXPERIMENT_SAMPLE,
  COMM_DETECT_MOTOR_PARAM,
  COMM_DETECT_MOTOR_R_L,
  COMM_DETECT_MOTOR_FLUX_LINKAGE,
  COMM_DETECT_ENCODER,
  COMM_DETECT_HALL_FOC,
  COMM_REBOOT,
  COMM_ALIVE,
  COMM_GET_DECODED_PPM,
  COMM_GET_DECODED_ADC,
  COMM_GET_DECODED_CHUK,
  COMM_FORWARD_CAN,
  COMM_SET_CHUCK_DATA,
  COMM_CUSTOM_APP_DATA,
  COMM_NRF_START_PAIRING
} COMM_PACKET_ID;

VescCommands::VescCommands(HardwareSerial &ser)
    : ser_(ser)
{
}

void VescCommands::begin(uint32_t baud) {
    ser_.begin(baud);
    inBufPtr_ = 0;
    mode_ = ModeNone;
    modeValue_ = 0;
    newValues_ = false;
    memset(&vals_, 0, sizeof(vals_));
    lastPoll_ = 0;
    lastCmd_ = 0;
}

void VescCommands::setHandbrake(uint16_t h1000) {
    modeValue_ = h1000;
    mode_ = ModeHandbrake;
}

void VescCommands::setRPM(int32_t rpm) {
    modeValue_ = rpm;
    mode_ = ModeRPM;
}

void VescCommands::setCurrentBrake(int32_t mA) {
    modeValue_ = mA;
    mode_ = ModeCurrentBrake;
}

void VescCommands::setCurrent(int32_t mA) {
    modeValue_ = mA;
    mode_ = ModeCurrent;
}

void VescCommands::setDuty(int32_t d100000) {
    modeValue_ = d100000;
    mode_ = ModeDuty;
}

void VescCommands::setNull() {
    mode_ = ModeNone;
}

bool VescCommands::gotNewValues() {
    bool r = newValues_;
    if (r) {
        newValues_ = false;
    }
    return r;
}

void VescCommands::poll(uint32_t now) {
    if (lastPoll_ == 0 || (now - lastPoll_ >= 200)) {
        //  only send the poll command with 5 ms spacing to cmd
        if (now - lastCmd_ >= 5) {
            //  send poll command
            pure_cmd(COMM_GET_VALUES);
            lastPoll_ = now;
        }
    } else if ((lastCmd_ == 0 || (now - lastCmd_ >= 10)) && (now - lastPoll_ >= 5)) {
        //  only send regular command with 5 ms spacing to poll
        lastCmd_ = now;
        switch (mode_) {
            case ModeHandbrake:
                int32_cmd(COMM_SET_HANDBRAKE, (int32_t)modeValue_);
                break;
            case ModeRPM:
                int32_cmd(COMM_SET_RPM, (int32_t)modeValue_);
                break;
            case ModeCurrentBrake:
                int32_cmd(COMM_SET_CURRENT_BRAKE, (int32_t)modeValue_);
                break;
            case ModeCurrent:
                int32_cmd(COMM_SET_CURRENT, (int32_t)modeValue_);
                break;
            case ModeDuty:
                int32_cmd(COMM_SET_DUTY, (int32_t)modeValue_);
                break;
            default:
                //  send nothing
                break;
        }
    }
    while (ser_.available()) {
        uint8_t b = ser_.read();
        if (inBufPtr_ == 0) {
            if (b != 2 && b != 3) {
                //  skip this byte
                continue;
            }
            inBuf_[0] = b;
            inBufPtr_ = 1;
        } else {
            inBuf_[inBufPtr_++] = b;
            if (inBuf_[0] == 2) {
                if (inBufPtr_ == 2) {
                    if (inBuf_[1] > sizeof(inBuf_) - 2 - 3) {
                        inBufPtr_ = 0;
                        continue;
                    }
                } else if (inBufPtr_ == 2 + inBuf_[1] + 3) {
                    decode_packet(&inBuf_[3], ((uint16_t)inBuf_[1] << 8) + inBuf_[2]);
                    inBufPtr_ = 0;
                }
            } else { // inBufPtr_[0] == 3
                if (inBufPtr_ == 3) {
                    if (((uint16_t)inBuf_[1] << 8) + inBuf_[2] > sizeof(inBuf_) - 3 - 3) {
                        inBufPtr_ = 0;
                        continue;
                    }
                } else if (inBufPtr_ == 3 + ((uint16_t)inBuf_[1] << 8) + inBuf_[2] + 3) {
                    decode_packet(&inBuf_[3], ((uint16_t)inBuf_[1] << 8) + inBuf_[2]);
                    inBufPtr_ = 0;
                }
            }
        }
    }
}

static int32_t get_be_i32(unsigned char const *&ptr) {
    uint32_t r = ((uint32_t)ptr[0] << 24) + ((uint32_t)ptr[1] << 16) +
        ((uint32_t)ptr[2] << 8) + ((uint32_t)ptr[3]);
    ptr += 4;
    return (int32_t)r;
}

static float get_be_f32(unsigned char const *&ptr, float scale) {
    int32_t i = get_be_i32(ptr);
    return i / scale;
}

static int16_t get_be_i16(unsigned char const *&ptr) {
    uint16_t r = ((uint16_t)ptr[0] << 8) + ((uint16_t)ptr[1]);
    ptr += 2;
    return (int16_t)r;
}

static float get_be_f16(unsigned char const *&ptr, float scale) {
    int16_t i = get_be_i16(ptr);
    return i / scale;
}

static void put_be_i32(unsigned char *&ptr, int32_t value) {
    ptr[0] = (uint8_t)((uint32_t)value >> 24);
    ptr[1] = (uint8_t)((uint32_t)value >> 16);
    ptr[2] = (uint8_t)((uint32_t)value >> 8);
    ptr[3] = (uint8_t)((uint32_t)value);
    ptr += 4;
}

static void put_be_u16(unsigned char *&ptr, uint16_t value) {
    ptr[0] = (uint8_t)((uint32_t)value >> 8);
    ptr[1] = (uint8_t)((uint32_t)value);
    ptr += 2;
}


bool VescCommands::decode_packet(unsigned char const *ptr, uint16_t size) {
    if (size < 3) {
        return false;
    }
    if (ptr[size-1] != 3) {
        return false;
    }
    uint16_t crc = crc16(ptr, size-3);
    uint16_t bufCrc = ((uint16_t)ptr[size-3] << 8) + ptr[size-2];
    if (crc != bufCrc) {
        return false;
    }
    if (size > 3) {
        switch (ptr[0]) {
            case COMM_GET_VALUES:
                if (size >= 1+12*4+4*2+1) {
                    vals_.tempFET_10 = get_be_f16(ptr, 10.0f);
                    vals_.tempMotor_10 = get_be_f16(ptr, 10.0f);
                    vals_.avgMotorCurrent_100 = get_be_f32(ptr, 100.0f);
                    vals_.avgInputCurrent_100 = get_be_f32(ptr, 100.0f);
                    vals_.avgId_100 = get_be_f32(ptr, 100.0f);
                    vals_.avgIq_100 = get_be_f32(ptr, 100.0f);
                    vals_.dutyCycle_1000 = get_be_f16(ptr, 1000.0f);
                    vals_.rpm_1 = get_be_f32(ptr, 1.0f);
                    vals_.inputVoltage_10 = get_be_f16(ptr, 10.0f);
                    vals_.ampHours_10000 = get_be_f32(ptr, 10000.0f);
                    vals_.ampHoursCharged_10000 = get_be_f32(ptr, 10000.0f);
                    vals_.wattHours_10000 = get_be_f32(ptr, 10000.0f);
                    vals_.wattHoursCharged_10000 = get_be_f32(ptr, 10000.0f);
                    vals_.tachometer = get_be_i32(ptr);
                    vals_.tachometerAbs = get_be_i32(ptr);
                    vals_.fault = *ptr++;
                    vals_.pidPos_1000000 = get_be_f32(ptr, 1000000.0f);
                    return true;
                }
            default:
                return false;
        }
    }
}

void VescCommands::pure_cmd(uint8_t cbyt) {
    unsigned char cmd[6];
    cmd[0] = 2;
    cmd[1] = 1;
    cmd[2] = cbyt;
    unsigned char *p = &cmd[3];
    put_be_u16(p, crc16(&cmd[2], 1));
    cmd[5] = 3;
    ser_.write(cmd, 6);
}

void VescCommands::int32_cmd(uint8_t cbyt, int32_t val) {
    unsigned char cmd[10];
    cmd[0] = 2;
    cmd[1] = 5;
    cmd[2] = cbyt;
    unsigned char *p = &cmd[3];
    put_be_i32(p, val);
    put_be_u16(p, crc16(&cmd[2], 5));
    cmd[9] = 3;
    ser_.write(cmd, 10);
}


