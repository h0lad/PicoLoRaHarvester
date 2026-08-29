#ifndef __DRIVER_H__
#define __DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct device;

struct driver_api {
    int (*init)(const struct device *dev);
    int (*read)(const struct device *dev, void *buf);
};

struct device {
    const char *name;
    const struct driver_api *api;
    const void *cfg;
    void *data;
};

static inline int device_init(const struct device *dev)
{
    return dev->api->init(dev);
}

static inline int device_read(const struct device *dev, void *buf)
{
    return dev->api->read(dev, buf);
}

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_H__ */
