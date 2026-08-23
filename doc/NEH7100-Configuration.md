
## Base Configuration

### Register 0x00 (LVD, OVP)

| 0x00 | Stop (LVD) | Charge (OVP) | Storage |
|---|---|---|---|
| `0x65` | 2.8 V | 3.5 V | LiFePO4 |
| `0x8B` | 3.0 V | 4.1 V | LiPo indoor |
| `0xAA` | 3.2 V | 4.0 V | LiPo alpine |
| `0x09` | 2.2 V | 3.9 V | Na-ion normal / winter |
| `0x07` | 2.2 V | 3.7 V | Na-ion summer, LIC alpine |
| `0x08` | 2.2 V | 3.8 V | LIC indoor |
| `0x00` | 2.2 V | 2.7 V | EDLC 2.7 V |



### Register 0x01 (LDO)


| STM32 powered from | 7 | 6 | 5:3 | USB 200 / 100 / 50 / 10 mA |
|---|:-:|:-:|:-:|---|
| LDO pass-through | 1 | 0 | `101` | `0xAF` / `0xAD` / `0xAC` / `0xAB` |
| LDO 3.0 V | 0 | 0 | `101` | `0x2F` / `0x2D` / `0x2C` / `0x2B` |


## Batteries 

###  1. LiFePO4 1S

**HC pins** (`1` = `CSTORE`, `0` = GND)

| STM32 powered from | HC4 | HC3 | HC2 | HC1 | HC0 | Until I²C: charge / stop / LDO / USB |
|---|:-:|:-:|:-:|:-:|:-:|---|
| Battery, or LDO pass-through | 1 | 1 | 0 | 1 | 0 | 3.5 V / 2.5 V / pass-through / 150 mA |
| LDO 3.0 V | 0 | 1 | 0 | 1 | 0 | 3.5 V / 3.3 V / 3.0 V / 50 mA |

**I²C after `Chip_OK`**

| Climate | Charge | Stop | MPPT | 0x00 | 0x01 battery | 0x01 pass-through | 0x01 LDO 3.0 V | 0x05 |
|---|---|---|---|---|---|---|---|---|
| Normal | 3.50 V | 2.80 V | 1 s | `0x65` | `0x07` | `0xAF` | `0x2F` | `0x01` |
| Alpine | 3.50 V | 2.80 V | 0.5 s | `0x65` | `0x07` | `0xAF` | `0x2F` | `0x00` |


---

###  2. LiPo 1S

**HC pins**

| Use | STM32 powered from | HC4 | HC3 | HC2 | HC1 | HC0 | Until I²C: charge / stop / LDO / USB |
|---|---|:-:|:-:|:-:|:-:|:-:|---|
| Indoor | Battery + buck-boost | 0 | 0 | 1 | 1 | 1 | 4.2 V / 2.2 V / pass-through / 200 mA |
| Indoor | LDO 3.0 V | 0 | 0 | 0 | 0 | 1 | 4.2 V / 2.2 V / **3.0 V** / 200 mA |
| Alpine | Battery + buck-boost | 1 | 0 | 0 | 1 | 0 | 4.0 V / 3.3 V / pass-through / 100 mA |
| Alpine | LDO 3.0 V | 0 | 0 | 0 | 0 | 1 | 4.2 V / 2.2 V / **3.0 V** / 200 mA |

**I²C after `Chip_OK`**

| Climate | Charge | Stop | MPPT | 0x00 | 0x01 battery + buck-boost | 0x01 LDO 3.0 V | 0x05 |
|---|---|---|---|---|---|---|---|
| Normal | 4.10 V | 3.00 V | 1 s | `0x8B` | `0x07` | `0x2F` | `0x01` |
| Alpine | 4.00 V | 3.20 V | 0.5 s | `0xAA` | `0x05` | `0x2D` | `0x00` |

---

### 3. Na-ion 1S

**HC pins**

| HC4 | HC3 | HC2 | HC1 | HC0 | Until I²C: charge / stop / LDO / USB |
|:-:|:-:|:-:|:-:|:-:|---|
| 1 | 1 | 0 | 1 | 0 | 3.5 V / 2.5 V / pass-through / 150 mA |


**I²C after `Chip_OK`**

| Climate | Charge | Stop | MPPT | 0x00 | 0x01 | 0x05 |
|---|---|---|---|---|---|---|
| Normal | 3.90 V | 2.20 V | 1 s | `0x09` | `0x07` | `0x01` |
| Alpine summer | 3.70 V | 2.20 V | 0.5 s | `0x07` | `0x05` | `0x00` |
| Alpine winter | 3.90 V | 2.20 V | 0.5 s | `0x09` | `0x05` | `0x00` |

