#if !defined(Defines_h)
#define Defines_h

/* Select the undervoltage cutoff value */
// #define MINIMUM_VOLTAGE 5.0f //  For stock donkey NiMH batteries -- 5V dropout will kill before this
// #define MINIMUM_VOLTAGE 6.4f //  For 2S LiPo
#define MINIMUM_VOLTAGE 9.6f //  For 3S LiPo
// #define MINIMUM_VOLTAGE 12.8f //  For 4S LiPo


#define PIN_STEER_IN 11
#define PIN_THROTTLE_IN 12
#define PIN_MODE_IN 13
#define PIN_A_VOLTAGE_IN A2

#define PIN_STEER_OUT 5
#define PIN_THROTTLE_OUT 6
#define PIN_POWER_CONTROL 20

//#define RPI_SERIAL Serial3  //  UART
#define RPI_SERIAL Serial   //  USB
#define RPI_BAUD_RATE 115200

#define IBUS_SERIAL Serial2

#define NUM_BAD_VOLT_TO_TURN_OFF 20
//  ADC returns 4095 at 3.3V; voltage divider is 5.7:1, so 18.8V at 4095
#define VOLTAGE_FACTOR 0.0048f

#endif  //  Defines_h

