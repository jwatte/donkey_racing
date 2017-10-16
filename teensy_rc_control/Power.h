#if !defined(Power_h)
#define Power_h

float power_voltage();
void setup_power();
void update_power(uint32_t now);
void force_power_on();
void force_power_off();
void set_power_auto(uint32_t now);

#endif  //  Power_h

