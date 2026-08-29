#ifndef __ADC_IF_H__
#define __ADC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Battery level thresholds (mV) for the 0..254 LoRaWAN scale. */
#define VDD_BAT                                 4200UL
#define VDD_MIN                                 3000UL

void sys_init_measurement(void);
uint16_t sys_get_battery_level(void);
int16_t sys_get_temperature_level(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_IF_H__ */
