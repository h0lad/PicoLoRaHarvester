#ifndef __SHT3X_H__
#define __SHT3X_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "driver.h"
#include "stm32wlxx_hal.h"

struct sht3x_cfg {
    I2C_HandleTypeDef *bus;
    uint8_t addr;
};

struct sht3x_sample {
    int16_t temp_cd;
    uint16_t hum_permille;
};

extern const struct driver_api sht3x_api;

#define SHT3X_DEV_DEFINE(name_, bus_, addr_) \
    static const struct sht3x_cfg name_##_cfg = { .bus = bus_, .addr = addr_ }; \
    const struct device name_ = { \
        .name = #name_, .api = &sht3x_api, .cfg = &name_##_cfg, .data = NULL, \
    }

#ifdef __cplusplus
}
#endif

#endif /* __SHT3X_H__ */
