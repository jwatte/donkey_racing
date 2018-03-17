#if !defined(VescCommands_h)
#define VescCommands_h

#include <Arduino.h>

/* You will need to turn on the UART app in the 
 * VESC BLDC tool for this to work. Set baud rate 
 * to 115200.
 */
class VescCommands {
    public:
        VescCommands(HardwareSerial &ser);
        void begin(uint32_t baud);
        
        /* Set up the communication to continually send 
         * some command to the ESC. Whatever the last-set 
         * command is, will be sent when you poll(), about 
         * every 10 milliseconds (if you poll at least 
         * that often.)
         */
        void setHandbrake(uint16_t h1000);
        void setRPM(int32_t rpm);
        void setCurrentBrake(int32_t mA);
        void setCurrent(int32_t mA);
        void setDuty(int32_t d100000);
        /* setNull() will turn off sending commands. It 
         * will still send get-status requests. The ESC 
         * will go into timeout even when you poll for the 
         * status, if no command is sent.
         */
        void setNull();

        /* poll() will send whatever the last set command is, 
         * and will check whether there is a new status 
         * received from the ESC.
         */
        void poll(uint32_t now);

        /* If a new status is received, this function will 
         * return true, once per new received status. You 
         * can read the actual data from the vals_ member.
         */
        bool gotNewValues();

        /* poke at internals if you wish */
        HardwareSerial &ser_;
        uint32_t lastCmd_;
        uint32_t lastPoll_;

        struct Values {
            float tempFET_10;
            float tempMotor_10;
            float avgMotorCurrent_100;
            float avgInputCurrent_100;
            float avgId_100;
            float avgIq_100;
            float dutyCycle_1000;
            float rpm_1;
            float inputVoltage_10;
            float ampHours_10000;
            float ampHoursCharged_10000;
            float wattHours_10000;
            float wattHoursCharged_10000;
            int32_t tachometer;
            int32_t tachometerAbs;
            uint8_t fault;
            float pidPos_1000000;
        };
        /* These are the values reported by the ESC.
         */
        Values vals_;
        bool newValues_;

        enum Mode {
            ModeNone = 0,
            ModeHandbrake,
            ModeRPM,
            ModeCurrentBrake,
            ModeCurrent,
            ModeDuty
        };
        Mode mode_;
        float modeValue_;

    private:
        bool decode_packet(unsigned char const *ptr, uint16_t size);
        void pure_cmd(uint8_t cmd);
        void int32_cmd(uint8_t cmd, int32_t val);

        uint8_t inBuf_[128];
        uint8_t inBufPtr_;
};

#endif  //  VescCommands_h
