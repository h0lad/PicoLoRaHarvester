#ifndef __NEH7100_H__
#define __NEH7100_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "driver.h"
#include "stm32wlxx_hal.h"

struct neh7100_cfg {
    I2C_HandleTypeDef *bus;
    uint8_t addr;
};

struct neh7100_sample {
    uint8_t range;
    uint32_t current_na;
};

extern const struct driver_api neh7100_api;

#define NEH7100_DEV_DEFINE(name_, bus_, addr_) \
    static const struct neh7100_cfg name_##_cfg = { .bus = bus_, .addr = addr_ }; \
    const struct device name_ = { \
        .name = #name_, .api = &neh7100_api, .cfg = &name_##_cfg, .data = NULL, \
    }

#ifdef __cplusplus
}
#endif

#endif /* __NEH7100_H__ */
