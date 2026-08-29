#ifndef __NEH7100_H__
#define __NEH7100_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

struct neh7100_device;

struct neh7100_cfg {
    I2C_HandleTypeDef *bus;
    uint8_t addr;
};

struct neh7100_sample {
    uint8_t range;
    uint32_t current_na;
};

struct neh7100_api {
    int (*init)(const struct neh7100_device *dev);
    int (*read)(const struct neh7100_device *dev, struct neh7100_sample *sample);
};

struct neh7100_device {
    const char *name;
    const struct neh7100_api *api;
    const struct neh7100_cfg *cfg;
    void *data;
};

static inline int neh7100_init(const struct neh7100_device *dev)
{
    return dev->api->init(dev);
}

static inline int neh7100_read(const struct neh7100_device *dev, struct neh7100_sample *sample)
{
    return dev->api->read(dev, sample);
}

extern const struct neh7100_api neh7100_api;

#define NEH7100_DEV_DEFINE(name_, bus_, addr_) \
    static const struct neh7100_cfg name_##_cfg = { .bus = bus_, .addr = addr_ }; \
    const struct neh7100_device name_ = { \
        .name = #name_, .api = &neh7100_api, .cfg = &name_##_cfg, .data = NULL, \
    }

#ifdef __cplusplus
}
#endif

#endif /* __NEH7100_H__ */
