#ifndef __TELEMETRY_H__
#define __TELEMETRY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define TELEMETRY_PAYLOAD_SIZE        22U

typedef struct
{
  uint16_t current_min_ua;        /* NEH7100 charge current, window min [uA] */
  uint16_t current_avg_ua;        /* NEH7100 charge current, window avg [uA] */
  uint16_t current_max_ua;        /* NEH7100 charge current, window max [uA] */
  uint16_t voltage_avg_mv;        /* battery voltage, window avg [mV]        */
  int16_t  temperature_min;       /* SHT3X temperature, window min [0.1 C]   */
  int16_t  temperature_avg;       /* SHT3X temperature, window avg [0.1 C]   */
  int16_t  temperature_max;       /* SHT3X temperature, window max [0.1 C]   */
  uint16_t humidity_min;          /* SHT3X humidity, window min [0.1 %]      */
  uint16_t humidity_avg;          /* SHT3X humidity, window avg [0.1 %]      */
  uint16_t humidity_max;          /* SHT3X humidity, window max [0.1 %]      */
  int16_t  mcu_temperature;       /* MCU internal temperature, window avg [0.1 C] */
} telemetry_data_t;

void telemetry_init(void);
void telemetry_process(void);
void telemetry_dump(void);
uint32_t telemetry_sleep_cap_ms(void);
void telemetry_collect(telemetry_data_t *telemetry);
uint8_t telemetry_encode(const telemetry_data_t *telemetry, uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __TELEMETRY_H__ */
