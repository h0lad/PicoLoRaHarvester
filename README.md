!! Warning !! Untested layout yet.

![MeshtasticRouterNode](doc/PicoLoRaHarvester.png)

# PicoLoRaHarvester

## What is this thing?

This PCB integrates a [RAK3172 LoRa module](https://docs.rakwireless.com/Product-Categories/WisDuo/RAK3172-Module/Datasheet/) (STM32WLE5) with a [Nexperia NEH7100](https://assets.nexperia.com/documents/data-sheet/NEH7100.pdf) energy harvesting PMIC. 

Main target are solar-powered LoRa nodes that run unattended in off-grid locations.

The board can run Meshcore or Meshtastic firmware. Both need power-saving patches so the microcontroller can enter deep sleep.  As the modem has permanently activated RX (which consumes 6-9mA depending on TCXO or not) you must calculate you power budget extremely carefully due to the harvesting limitations.

## Features

* Energy harvesting via Nexperia NEH7100 PMIC: 15 μW to 100 mW input range, up to 95% efficiency
* LiPo-safe PMIC hardware defaults: OVP=4.2 V, LVD=3.2 V, LDO=3.0 V, OCP=200 mA (overridable via I²C)
* The PMIC's LDO supplies the RAK3172 with 3.0 V
* USB bypass charging (VUSB) at up to 200 mA via the UART connector
* Battery (ADC_LIPO) and solar (ADC_VBUS) voltage monitoring via 1:1 resistor dividers

## Physical Outlines

| Dimension | Value |
|-----------|-------|
| Width | 50.54 mm |
| Height | 45.50 mm |

Fits into this box: [Aliexpress, F-type, 63x58x35](https://de.aliexpress.com/item/1005005480970197.html).

Yes - the mounting hole is not symmetric. 

## Pinout

### Connectors

| Designator | Connector | Pins | Purpose |
|------------|-----------|------|---------|
| J2 | JST PH 2-pin | 1: VBUS, 2: GND | Solar panel input (feeds NEH7100 energy harvester) |
| J3 | JST PH 4-pin | 1: GND, 2: VUSB, 3: UART TX, 4: UART RX | UART + USB bypass charging (200 mA) |
| J4 | JST PH 2-pin | 1: VLIPO, 2: GND | LiPo battery |
| J5 | JST PH 4-pin | 1: GND, 2: +3V0, 3: I2C2 SCL, 4: I2C2 SDA | I²C expansion bus (I2C2) |
| J6 | JST PH 4-pin | 1: GND, 2: NRST, 3: SWD, 4: SWCLK | SWD debug/programming |

### RAK3172 Pin Mapping

| RAK3172 Pin | Function | Connected To |
|-------------|----------|-------------|
| PA2 | UART2 TX | J3 pin 3 |
| PA3 | UART2 RX | J3 pin 4 |
| PA11 | I2C1 SDA | NEH7100 SDA (PMIC config bus) |
| PA12 | I2C1 SCL | NEH7100 SCL (PMIC config bus) |
| PA13 | SWDIO | J6 pin 3 |
| PA14 | SWCLK | J6 pin 4 |
| PB3 / A0 | ADC | ADC_VBUS (solar voltage divider, 1 MΩ / 1 MΩ) |
| PB4 / A1 | ADC | ADC_LIPO (battery voltage divider, 1 MΩ / 1 MΩ) |
| BOOT0 | Bootloader | SW1 (SSSS811101 slide switch) via R1 (10 kΩ) |
| NRST | Reset | SW2 (push button), J6 pin 2 |

### I²C Bus Allocation

| Bus | Pins | Purpose |
|-----|------|---------|
| I2C1 | PA11 (SDA), PA12 (SCL) | NEH7100 PMIC telemetry and configuration |
| I2C2 | RAK3172 secondary I²C peripheral | J5 expansion connector for external sensors/peripherals |

Both I2C buses have on-board 10 kΩ pull-up resistors (R2/R9 for I2C1, R4/R10 for I2C2).

## Voltage Monitoring

| Net | Divider | Ratio | RAK3172 ADC Pin |
|-----|---------|-------|-----------------|
| ADC_VBUS | R13 (1 MΩ) / R14 (1 MΩ) + C15 (100 nF) | 1:2 | PB3 / A0 |
| ADC_LIPO | R15 (1 MΩ) / R16 (1 MΩ) + C16 (100 nF) | 1:2 | PB4 / A1 |

Both dividers halve the input voltage for safe ADC input range (~1.65 V max at 3.3 V full scale).

## Power Architecture

```
Solar Panel (J2) ── VBUS ── NEH7100 VIN (charge pump + MPPT)
USB 5V   (J3)   ── VUSB ── NEH7100 VUSB (bypass, 200 mA max)
LiPo     (J4)   ── VLIPO ── NEH7100 VBAT / VBATOK
                              │
                              ├─ Internal 4-stage charge pump
                              ├─ CSTORE bulk capacitor (47 μF, C5)
                              ├─ BLOAD load switch
                              └─ VLDO ── +3V0 rail (regulated 3.0 V)
                                            │
                                            ├─ RAK3172 VDD
                                            ├─ I2C pull-ups
                                            └─ J5 pin 2 (+3V0 output)
```


## FAQ

#### How much current can I expect from the NEH7100?

The NEH7100 has an input power range of 15 μW to 100 mW and efficiency up to 95% (per [datasheet](https://assets.nexperia.com/documents/data-sheet/NEH7100.pdf)). At the 3.0 V LDO output this translates to an absolute maximum of ~31 mA from harvested energy.

The USB input (VUSB on J3 pin 2) bypasses the charge pump and can supply up to 200 mA directly for bench use or supplemental charging.

#### Why the NEH7100 instead of the TI BQ25570?

* Voltage thresholds (OVP, LVD, LDO) are set by hard-code pins or I²C registers. The BQ25570 programs them with external resistor dividers, which adds resistor tolerance to every threshold and a permanent leakage path across the battery.
* The charge current can be read back over I²C (I_MEASURED/I_RANGE registers). The BQ25570 has no digital interface.
* External circuit consists of capacitors only. The BQ25570 needs an inductor each for its boost charger and buck converter.
* The pinout groups the flying caps, I²C and power pins by side, so routing needs no crossovers and there are no switching inductor loops to keep tight.


## License

CERN-OHL-S-2.0
