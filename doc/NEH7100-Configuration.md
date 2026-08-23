
## NEH7100 without LDO/Buck → STM32WLE

This uses the wide voltage input range of the STM32WLE. It perfectly fits a LiFePO4.

LDO in bypass: `VLDO` follows `VBAT`.

| Chemistry | Climate | HC\[4:0\] | HC live (OVP / LVD / USB) | `0x00` | `0x01` | `0x05` |
|---|---|---|---|---|---|---|
| LiFePO4 | mountain | `11010` | 3.50 / 2.50 / 150 mA | `0x65` 2.80 / 3.50 | `0x87` | `0x00` |
| LiFePO4 | indoor | `11010` | 3.50 / 2.50 / 150 mA | `0x36` 2.50 / 3.60 | `0x87` | `0x01` |

## NEH7100 with a connected Buck-Boost (3.3 V) → STM32WLE

LDO in bypass: `VLDO` follows `VBAT`. STM32 runs from the converter.

This might be needed if you are either using Na-ion (which has a useable voltage range below 3.0V) or even with a LiPo if you need the full 3.3V for unlimited TX power of the LoRa module.

| Chemistry | Climate | HC\[4:0\] | HC live (OVP / LVD / USB) | `0x00` | `0x01` | `0x05` |
|---|---|---|---|---|---|---|
| LiPo | mountain | `10010` | 4.00 / 3.30 / 100 mA | `0xAA` 3.20 / 4.00 | `0x87` | `0x00` |
| LiPo | indoor | `10010` | 4.00 / 3.30 / 100 mA | `0x8B` 3.00 / 4.10 | `0x87` | `0x01` |
| Na-ion | mountain summer | `10010` | 4.00 / 3.30 / 100 mA | `0x08` 2.20 / 3.80 | `0x87` | `0x00` |
| Na-ion | mountain winter | `10010` | 4.00 / 3.30 / 100 mA | `0x07` 2.20 / 3.70 | `0x87` | `0x00` |
| Na-ion | indoor | `10010` | 4.00 / 3.30 / 100 mA | `0x09` 2.20 / 3.90 | `0x87` | `0x01` |

## NEH7100 with internal LDO → STM32WLE

LDO set to 3.0 V. STM32 runs from `VLDO`. The "budget" variant.

| Chemistry | Climate | HC\[4:0\] | HC live (OVP / LVD / USB) | `0x00` | `0x01` | `0x05` |
|---|---|---|---|---|---|---|
| LiPo | mountain | `00001` | 4.20 / 3.20 / 200 mA | `0xAA` 3.20 / 4.00 | `0x2F` | `0x00` |
| LiPo | indoor | `00001` | 4.20 / 3.20 / 200 mA | `0x8B` 3.00 / 4.10 | `0x2F` | `0x01` |
