#ifndef __SHT3X_H__
#define __SHT3X_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

struct sht3x_device;

struct sht3x_cfg {
    I2C_HandleTypeDef *bus;
    uint8_t addr;
};

struct sht3x_sample {
    int16_t temp_cd;
    uint16_t hum_permille;
};

struct sht3x_api {
    int (*init)(const struct sht3x_device *dev);
    int (*read)(const struct sht3x_device *dev, struct sht3x_sample *sample);
};

struct sht3x_device {
    const char *name;
    const struct sht3x_api *api;
    const struct sht3x_cfg *cfg;
    void *data;
};

static inline int sht3x_init(const struct sht3x_device *dev)
{
    return dev->api->init(dev);
}

static inline int sht3x_read(const struct sht3x_device *dev, struct sht3x_sample *sample)
{
    return dev->api->read(dev, sample);
}

extern const struct sht3x_api sht3x_api;

#define SHT3X_DEV_DEFINE(name_, bus_, addr_) \
    static const struct sht3x_cfg name_##_cfg = { .bus = bus_, .addr = addr_ }; \
    const struct sht3x_device name_ = { \
        .name = #name_, .api = &sht3x_api, .cfg = &name_##_cfg, .data = NULL, \
    }

#ifdef __cplusplus
}
#endif

#endif /* __SHT3X_H__ */
