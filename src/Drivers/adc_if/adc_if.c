#include "adc_if.h"
#include "main.h"
#include "adc.h"

extern ADC_HandleTypeDef hadc;

#define ADC_MAX_COUNT            4095UL

/* Battery divider R13 (390k) / R14 (1M): Vbat = Vadc * 1390 / 1000 */
#define BAT_DIVIDER_NUMERATOR    1390UL
#define BAT_DIVIDER_DENOMINATOR  1000UL

/* C15 (100nF) settling through the divider once the MOSFET is on */
#define BAT_ADC_SETTLE_MS        200UL

#define TEMPSENSOR_TYP_CAL1_V    ((int32_t) 760)
#define TEMPSENSOR_TYP_AVGSLOPE  ((int32_t) 2500)

static uint32_t adc_read_channel(uint32_t channel);
static uint32_t get_vdda_millivolt(void);

void sys_init_measurement(void)
{
  hadc.Instance = ADC;
}

uint16_t sys_get_battery_level(void)
{
  uint32_t vdda_mv;
  uint32_t adc_raw;
  uint32_t battery_mv;

  vdda_mv = get_vdda_millivolt();

  HAL_GPIO_WritePin(EN_VOL_DIVIDER_GPIO_Port, EN_VOL_DIVIDER_Pin, GPIO_PIN_RESET);
  HAL_Delay(BAT_ADC_SETTLE_MS);

  adc_raw = adc_read_channel(ADC_CHANNEL_3);

  HAL_GPIO_WritePin(EN_VOL_DIVIDER_GPIO_Port, EN_VOL_DIVIDER_Pin, GPIO_PIN_SET);

  battery_mv = (uint32_t)(((uint64_t)adc_raw * vdda_mv * BAT_DIVIDER_NUMERATOR) /
                          ((uint64_t)ADC_MAX_COUNT * BAT_DIVIDER_DENOMINATOR));

  return (uint16_t)battery_mv;
}

int16_t sys_get_temperature_level(void)
{
  uint32_t vdda_mv;
  uint32_t ts_raw;
  int32_t  temperature;

  vdda_mv = get_vdda_millivolt();
  ts_raw = adc_read_channel(ADC_CHANNEL_TEMPSENSOR);

  if (((int32_t)*TEMPSENSOR_CAL2_ADDR - (int32_t)*TEMPSENSOR_CAL1_ADDR) != 0)
  {
    temperature = __LL_ADC_CALC_TEMPERATURE(vdda_mv, ts_raw, LL_ADC_RESOLUTION_12B);
  }
  else
  {
    temperature = __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(TEMPSENSOR_TYP_AVGSLOPE,
                                                       TEMPSENSOR_TYP_CAL1_V,
                                                       TEMPSENSOR_CAL1_TEMP,
                                                       vdda_mv,
                                                       ts_raw,
                                                       LL_ADC_RESOLUTION_12B);
  }

  return (int16_t)(temperature * 256);
}

static uint32_t adc_read_channel(uint32_t channel)
{
  uint32_t adc_converted_value = 0;
  ADC_ChannelConfTypeDef sConfig = {0};

  MX_ADC_Init();

  if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel      = channel;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_Start(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }

  adc_converted_value = HAL_ADC_GetValue(&hadc);

  HAL_ADC_Stop(&hadc);
  HAL_ADC_DeInit(&hadc);

  return adc_converted_value;
}

static uint32_t get_vdda_millivolt(void)
{
  uint32_t vdda_mv = 0;
  uint32_t vrefint_raw;

  vrefint_raw = adc_read_channel(ADC_CHANNEL_VREFINT);

  if (vrefint_raw == 0)
  {
    vdda_mv = 0;
  }
  else if ((uint32_t)*VREFINT_CAL_ADDR != 0xFFFFU)
  {
    vdda_mv = __LL_ADC_CALC_VREFANALOG_VOLTAGE(vrefint_raw, LL_ADC_RESOLUTION_12B);
  }
  else
  {
    vdda_mv = (VREFINT_CAL_VREF * 1510UL) / vrefint_raw;
  }

  return vdda_mv;
}
