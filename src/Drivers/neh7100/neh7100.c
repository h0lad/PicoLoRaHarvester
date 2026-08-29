#include "neh7100.h"

#define REG_CHIP_ID    0x07
#define REG_I_RANGE    0x09
#define REG_I_MEASURED 0x0A

#define CHIP_ID        0x15

/* I_RANGE to charge current scale, 0.1 nA per LSB */
static const uint32_t scale_q1_na[4] = { 706, 4780, 47100, 675000 };

static int neh7100_init(const struct device *dev)
{
    const struct neh7100_cfg *cfg = dev->cfg;
    uint8_t id;

    if (HAL_I2C_Mem_Read(cfg->bus, cfg->addr << 1, REG_CHIP_ID,
                         I2C_MEMADD_SIZE_8BIT, &id, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        return -1;
    }

    return id == CHIP_ID ? 0 : -2;
}

static int neh7100_read(const struct device *dev, void *buf)
{
    const struct neh7100_cfg *cfg = dev->cfg;
    struct neh7100_sample *s = buf;
    uint8_t reg[2];

    if (HAL_I2C_Mem_Read(cfg->bus, cfg->addr << 1, REG_I_RANGE,
                         I2C_MEMADD_SIZE_8BIT, reg, 2, HAL_MAX_DELAY) != HAL_OK)
    {
        return -1;
    }

    s->range = reg[0] & 0x03U;
    s->current_na = ((uint32_t)reg[1] * scale_q1_na[s->range]) / 10U;

    return 0;
}

const struct driver_api neh7100_api = {
    .init = neh7100_init,
    .read = neh7100_read,
};
