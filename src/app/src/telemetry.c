#include "telemetry.h"
#include "i2c.h"
#include "neh7100.h"
#include "sht3x.h"
#include "adc_if.h"
#include "stm32_systime.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FAST_PERIOD_MS   30000UL
#define SLOW_PERIOD_MS   300000UL
#define DUMP_EVERY_N_CALLS   30UL

NEH7100_DEV_DEFINE(neh7100, &hi2c2, 0x3C);
SHT3X_DEV_DEFINE(sht3x, &hi2c1, 0x44);

static struct
{
  uint32_t last_fast_ms;
  uint32_t last_slow_ms;

  uint16_t v_min_mv;
  uint16_t v_max_mv;
  uint32_t v_sum_mv;
  uint32_t v_count;

  uint32_t c_min_na;
  uint32_t c_max_na;
  uint32_t c_sum_na;
  uint32_t c_count;

  int16_t t_min_cd;
  int16_t t_max_cd;
  int32_t t_sum_cd;
  uint32_t t_count;

  uint16_t h_min_p;
  uint16_t h_max_p;
  uint32_t h_sum_p;
  uint32_t h_count;

  int16_t m_min_cd;
  int16_t m_max_cd;
  int32_t m_sum_cd;
  uint32_t m_count;
} agg;

static uint32_t now_ms(void)
{
  return SysTimeToMs(SysTimeGetMcuTime());
}

static void sample_fast(void)
{
  struct neh7100_sample n;
  uint16_t v = sys_get_battery_level();

  if (agg.v_count == 0)
  {
    agg.v_min_mv = v;
    agg.v_max_mv = v;
  }
  else if (v < agg.v_min_mv)
  {
    agg.v_min_mv = v;
  }
  else if (v > agg.v_max_mv)
  {
    agg.v_max_mv = v;
  }
  agg.v_sum_mv += v;
  agg.v_count++;

  if (device_read(&neh7100, &n) == 0)
  {
    if (agg.c_count == 0)
    {
      agg.c_min_na = n.current_na;
      agg.c_max_na = n.current_na;
    }
    else if (n.current_na < agg.c_min_na)
    {
      agg.c_min_na = n.current_na;
    }
    else if (n.current_na > agg.c_max_na)
    {
      agg.c_max_na = n.current_na;
    }
    agg.c_sum_na += n.current_na;
    agg.c_count++;
  }
}

static void sample_slow(void)
{
  struct sht3x_sample s;
  int16_t mcu_cd;

  if (device_read(&sht3x, &s) == 0)
  {
    if (agg.t_count == 0)
    {
      agg.t_min_cd = s.temp_cd;
      agg.t_max_cd = s.temp_cd;
    }
    else if (s.temp_cd < agg.t_min_cd)
    {
      agg.t_min_cd = s.temp_cd;
    }
    else if (s.temp_cd > agg.t_max_cd)
    {
      agg.t_max_cd = s.temp_cd;
    }
    agg.t_sum_cd += s.temp_cd;
    agg.t_count++;

    if (agg.h_count == 0)
    {
      agg.h_min_p = s.hum_permille;
      agg.h_max_p = s.hum_permille;
    }
    else if (s.hum_permille < agg.h_min_p)
    {
      agg.h_min_p = s.hum_permille;
    }
    else if (s.hum_permille > agg.h_max_p)
    {
      agg.h_max_p = s.hum_permille;
    }
    agg.h_sum_p += s.hum_permille;
    agg.h_count++;
  }

  mcu_cd = (int16_t)((sys_get_temperature_level() * 10) / 256);
  if (agg.m_count == 0)
  {
    agg.m_min_cd = mcu_cd;
    agg.m_max_cd = mcu_cd;
  }
  else if (mcu_cd < agg.m_min_cd)
  {
    agg.m_min_cd = mcu_cd;
  }
  else if (mcu_cd > agg.m_max_cd)
  {
    agg.m_max_cd = mcu_cd;
  }
  agg.m_sum_cd += mcu_cd;
  agg.m_count++;
}

void telemetry_init(void)
{
  memset(&agg, 0, sizeof(agg));

  device_init(&neh7100);
  device_init(&sht3x);

  agg.last_fast_ms = now_ms();
  agg.last_slow_ms = agg.last_fast_ms;
}

void telemetry_process(void)
{
  uint32_t now = now_ms();
  static uint32_t dump_call_cnt = 0;

  if ((uint32_t)(now - agg.last_fast_ms) >= FAST_PERIOD_MS)
  {
    sample_fast();
    agg.last_fast_ms = now;
  }

  if ((uint32_t)(now - agg.last_slow_ms) >= SLOW_PERIOD_MS)
  {
    sample_slow();
    agg.last_slow_ms = now;
  }

  if (++dump_call_cnt >= DUMP_EVERY_N_CALLS)
  {
    telemetry_dump();
    dump_call_cnt = 0;
  }
}

void telemetry_dump(void)
{
  struct neh7100_sample n;
  struct sht3x_sample s;
  uint16_t vbat = sys_get_battery_level();
  int16_t mcu_cd = (int16_t)((sys_get_temperature_level() * 10) / 256);

  if (device_read(&neh7100, &n) == 0)
  {
    printf("dump: cur %lu nA (range %u)\r\n", (unsigned long)n.current_na, n.range);
  }
  else
  {
    printf("dump: cur FAIL\r\n");
  }

  if (device_read(&sht3x, &s) == 0)
  {
    printf("dump: temp %d.%d C  hum %u.%u %%\r\n",
           s.temp_cd / 10, abs(s.temp_cd % 10),
           s.hum_permille / 10, s.hum_permille % 10);
  }
  else
  {
    printf("dump: temp/hum FAIL\r\n");
  }

  printf("dump: vbat %u mV  mcu %d.%d C\r\n", vbat, mcu_cd / 10, abs(mcu_cd % 10));
}

uint32_t telemetry_sleep_cap_ms(void)
{
  uint32_t now = now_ms();
  uint32_t fast_elapsed = (uint32_t)(now - agg.last_fast_ms);
  uint32_t slow_elapsed = (uint32_t)(now - agg.last_slow_ms);
  uint32_t cap;

  if (fast_elapsed >= FAST_PERIOD_MS)
  {
    return 0;
  }
  cap = FAST_PERIOD_MS - fast_elapsed;

  if (slow_elapsed >= SLOW_PERIOD_MS)
  {
    return 0;
  }
  if ((SLOW_PERIOD_MS - slow_elapsed) < cap)
  {
    cap = SLOW_PERIOD_MS - slow_elapsed;
  }

  return cap;
}

void telemetry_collect(telemetry_data_t *telemetry)
{
  uint32_t now;

  if (telemetry == NULL)
  {
    return;
  }

  telemetry->current_min_ua = (uint16_t)(agg.c_min_na / 1000UL);
  telemetry->current_avg_ua = (uint16_t)((agg.c_count != 0) ? (agg.c_sum_na / agg.c_count / 1000UL) : 0);
  telemetry->current_max_ua = (uint16_t)(agg.c_max_na / 1000UL);

  telemetry->voltage_avg_mv = (uint16_t)((agg.v_count != 0) ? (agg.v_sum_mv / agg.v_count) : 0);

  telemetry->temperature_min = agg.t_min_cd;
  telemetry->temperature_avg = (int16_t)((agg.t_count != 0) ? (agg.t_sum_cd / (int32_t)agg.t_count) : 0);
  telemetry->temperature_max = agg.t_max_cd;

  telemetry->humidity_min = agg.h_min_p;
  telemetry->humidity_avg = (uint16_t)((agg.h_count != 0) ? (agg.h_sum_p / agg.h_count) : 0);
  telemetry->humidity_max = agg.h_max_p;

  telemetry->mcu_temperature = (int16_t)((agg.m_count != 0) ? (agg.m_sum_cd / (int32_t)agg.m_count) : 0);

  now = now_ms();
  memset(&agg, 0, sizeof(agg));
  agg.last_fast_ms = now;
  agg.last_slow_ms = now;
}

uint8_t telemetry_encode(const telemetry_data_t *telemetry, uint8_t *buffer)
{
  if ((telemetry == NULL) || (buffer == NULL))
  {
    return 0;
  }

  buffer[0]  = (uint8_t)(telemetry->current_min_ua >> 8);
  buffer[1]  = (uint8_t)(telemetry->current_min_ua);
  buffer[2]  = (uint8_t)(telemetry->current_avg_ua >> 8);
  buffer[3]  = (uint8_t)(telemetry->current_avg_ua);
  buffer[4]  = (uint8_t)(telemetry->current_max_ua >> 8);
  buffer[5]  = (uint8_t)(telemetry->current_max_ua);
  buffer[6]  = (uint8_t)(telemetry->voltage_avg_mv >> 8);
  buffer[7]  = (uint8_t)(telemetry->voltage_avg_mv);
  buffer[8]  = (uint8_t)((uint16_t)telemetry->temperature_min >> 8);
  buffer[9]  = (uint8_t)(telemetry->temperature_min);
  buffer[10] = (uint8_t)((uint16_t)telemetry->temperature_avg >> 8);
  buffer[11] = (uint8_t)(telemetry->temperature_avg);
  buffer[12] = (uint8_t)((uint16_t)telemetry->temperature_max >> 8);
  buffer[13] = (uint8_t)(telemetry->temperature_max);
  buffer[14] = (uint8_t)(telemetry->humidity_min >> 8);
  buffer[15] = (uint8_t)(telemetry->humidity_min);
  buffer[16] = (uint8_t)(telemetry->humidity_avg >> 8);
  buffer[17] = (uint8_t)(telemetry->humidity_avg);
  buffer[18] = (uint8_t)(telemetry->humidity_max >> 8);
  buffer[19] = (uint8_t)(telemetry->humidity_max);
  buffer[20] = (uint8_t)((uint16_t)telemetry->mcu_temperature >> 8);
  buffer[21] = (uint8_t)(telemetry->mcu_temperature);

  return TELEMETRY_PAYLOAD_SIZE;
}
