#include "sht3x.h"

#define CMD_MEAS_HIGH  0x2C06

static uint8_t crc8(const uint8_t *p, uint8_t len)
{
    uint8_t crc = 0xFF;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

static int sht3x_api_init(const struct sht3x_device *dev)
{
    const struct sht3x_cfg *cfg = dev->cfg;
    uint8_t cmd[2] = { (uint8_t)(CMD_MEAS_HIGH >> 8), (uint8_t)(CMD_MEAS_HIGH & 0xFF) };

    if (HAL_I2C_Master_Transmit(cfg->bus, cfg->addr << 1, cmd, 2, HAL_MAX_DELAY) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

static int sht3x_api_read(const struct sht3x_device *dev, struct sht3x_sample *sample)
{
    const struct sht3x_cfg *cfg = dev->cfg;
    uint8_t cmd[2] = { (uint8_t)(CMD_MEAS_HIGH >> 8), (uint8_t)(CMD_MEAS_HIGH & 0xFF) };
    uint8_t data[6];
    uint16_t raw_t, raw_h;

    if (HAL_I2C_Master_Transmit(cfg->bus, cfg->addr << 1, cmd, 2, HAL_MAX_DELAY) != HAL_OK)
    {
        return -1;
    }

    if (HAL_I2C_Master_Receive(cfg->bus, cfg->addr << 1, data, 6, HAL_MAX_DELAY) != HAL_OK)
    {
        return -1;
    }

    if (crc8(&data[0], 2) != data[2] || crc8(&data[3], 2) != data[5])
    {
        return -2;
    }

    raw_t = (uint16_t)((data[0] << 8) | data[1]);
    raw_h = (uint16_t)((data[3] << 8) | data[4]);

    sample->temp_cd = (int16_t)((1750U * raw_t) / 65535U) - 450;
    sample->hum_permille = (uint16_t)((1000U * raw_h) / 65535U);

    return 0;
}

const struct sht3x_api sht3x_api = {
    .init = sht3x_api_init,
    .read = sht3x_api_read,
};
