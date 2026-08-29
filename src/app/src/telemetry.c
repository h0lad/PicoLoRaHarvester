#include "telemetry.h"
#include "i2c.h"
#include "neh7100.h"
#include "sht3x.h"
#include "adc_if.h"
#include "stm32_systime.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FAST_PERIOD_MS   1000UL
#define SHT3X_PERIOD_MS  10000UL
#define DUMP_EVERY_N_CALLS   30UL

NEH7100_DEV_DEFINE(neh7100, &hi2c2, 0x3C);
SHT3X_DEV_DEFINE(sht3x, &hi2c1, 0x44);

static struct
{
  uint16_t v_min_mv;
  uint16_t v_max_mv;
  uint32_t v_sum_mv;
  uint32_t v_count;

  uint32_t c_min_na;
  uint32_t c_max_na;
  uint32_t c_sum_na;
  uint32_t c_count;

  int16_t t_sum_cd;
  uint32_t t_count;

  uint32_t h_sum_p;
  uint32_t h_count;

  int32_t m_sum_cd;
  uint32_t m_count;
} agg;

static uint32_t last_fast_ms;
static uint32_t last_sht3x_ms;

static uint32_t now_ms(void)
{
  return SysTimeToMs(SysTimeGetMcuTime());
}

static bool agg_complete(void)
{
  return (agg.v_count != 0) && (agg.c_count != 0) && (agg.t_count != 0) &&
         (agg.h_count != 0) && (agg.m_count != 0);
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

  if (neh7100_read(&neh7100, &n) == 0)
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

static void sample_sht3x(void)
{
  struct sht3x_sample s;

  if (sht3x_read(&sht3x, &s) == 0)
  {
    agg.t_sum_cd += s.temp_cd;
    agg.t_count++;
    agg.h_sum_p += s.hum_permille;
    agg.h_count++;
  }

  agg.m_sum_cd += (int16_t)((sys_get_temperature_level() * 10) / 256);
  agg.m_count++;
}

void telemetry_init(void)
{
  memset(&agg, 0, sizeof(agg));
  neh7100_init(&neh7100);
  sht3x_init(&sht3x);
  last_fast_ms = now_ms();
  last_sht3x_ms = last_fast_ms;
}

void telemetry_process(void)
{
  uint32_t now = now_ms();
  static uint32_t dump_call_cnt = 0;

  if ((uint32_t)(now - last_fast_ms) >= FAST_PERIOD_MS)
  {
    sample_fast();
    last_fast_ms = now;
  }

  if ((uint32_t)(now - last_sht3x_ms) >= SHT3X_PERIOD_MS)
  {
    sample_sht3x();
    last_sht3x_ms = now;
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

  if (neh7100_read(&neh7100, &n) == 0)
  {
    printf("dump: cur %lu nA (range %u)\r\n", (unsigned long)n.current_na, n.range);
  }
  else
  {
    printf("dump: cur FAIL\r\n");
  }

  if (sht3x_read(&sht3x, &s) == 0)
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
  uint32_t fast_elapsed = (uint32_t)(now - last_fast_ms);
  uint32_t sht3x_elapsed = (uint32_t)(now - last_sht3x_ms);
  uint32_t cap;

  if (fast_elapsed >= FAST_PERIOD_MS)
  {
    return 0;
  }
  cap = FAST_PERIOD_MS - fast_elapsed;

  if (sht3x_elapsed >= SHT3X_PERIOD_MS)
  {
    return 0;
  }
  if ((SHT3X_PERIOD_MS - sht3x_elapsed) < cap)
  {
    cap = SHT3X_PERIOD_MS - sht3x_elapsed;
  }

  return cap;
}

static void agg_to_telemetry(telemetry_data_t *telemetry)
{
  telemetry->current_min_ua = (uint16_t)(agg.c_min_na / 1000UL);
  telemetry->current_avg_ua = (uint16_t)(agg.c_sum_na / agg.c_count / 1000UL);
  telemetry->current_max_ua = (uint16_t)(agg.c_max_na / 1000UL);

  telemetry->voltage_min_mv = agg.v_min_mv;
  telemetry->voltage_avg_mv = (uint16_t)(agg.v_sum_mv / agg.v_count);
  telemetry->voltage_max_mv = agg.v_max_mv;

  telemetry->temperature_avg = (int16_t)(agg.t_sum_cd / (int32_t)agg.t_count);
  telemetry->humidity_avg = (uint16_t)(agg.h_sum_p / agg.h_count);
  telemetry->mcu_temperature = (int16_t)(agg.m_sum_cd / (int32_t)agg.m_count);
}

bool telemetry_collect(telemetry_data_t *telemetry)
{
  if ((telemetry == NULL) || !agg_complete())
  {
    return false;
  }

  agg_to_telemetry(telemetry);
  memset(&agg, 0, sizeof(agg));
  return true;
}

static void put_bits(uint8_t *buffer, uint32_t value, uint8_t nbits, uint32_t *bitpos)
{
  while (nbits > 0)
  {
    uint8_t byte_index = (uint8_t)(*bitpos >> 3);
    uint8_t bit_index  = (uint8_t)(*bitpos & 0x07U);
    uint8_t room = 8U - bit_index;
    uint8_t take = (nbits < room) ? nbits : room;
    uint8_t shift = nbits - take;

    buffer[byte_index] |= (uint8_t)(((value >> shift) & ((1U << take) - 1U)) << (room - take));

    *bitpos += take;
    nbits -= take;
  }
}

uint8_t telemetry_encode(const telemetry_data_t *telemetry, uint8_t *buffer)
{
  uint32_t bitpos = 0;

  if ((telemetry == NULL) || (buffer == NULL))
  {
    return 0;
  }

  memset(buffer, 0, TELEMETRY_PAYLOAD_SIZE);

  put_bits(buffer, telemetry->current_min_ua & 0x7FFFu, 15, &bitpos);
  put_bits(buffer, telemetry->current_avg_ua & 0x7FFFu, 15, &bitpos);
  put_bits(buffer, telemetry->current_max_ua & 0x7FFFu, 15, &bitpos);
  put_bits(buffer, telemetry->voltage_min_mv & 0x1FFFu, 13, &bitpos);
  put_bits(buffer, telemetry->voltage_avg_mv & 0x1FFFu, 13, &bitpos);
  put_bits(buffer, telemetry->voltage_max_mv & 0x1FFFu, 13, &bitpos);
  put_bits(buffer, (uint16_t)telemetry->temperature_avg & 0x0FFFu, 12, &bitpos);
  put_bits(buffer, telemetry->humidity_avg & 0x03FFu, 10, &bitpos);
  put_bits(buffer, (uint16_t)telemetry->mcu_temperature & 0x0FFFu, 12, &bitpos);

  return TELEMETRY_PAYLOAD_SIZE;
}
