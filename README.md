# STM32 NUCLEO-F446RE – BME280 Projects

Two small embedded projects built step by step on the **NUCLEO-F446RE** board with **STM32CubeIDE** and the **HAL** library. Both read a **BME280** (or BMP280) environmental sensor over **I²C** and report the results over **UART**. The BME280 driver is written from scratch (no external library), including the integer compensation formulas from the Bosch datasheet.

| Project | Where | What it does |
|---|---|---|
| **1. BME280 Sensor Reader** | `main` branch | Reads temperature, pressure and humidity and prints them to a serial terminal once per second |
| **2. BME280 with Buzzer Alarm** | [`BME280-with-Buzzer-Alarm`](../../tree/BME280-with-Buzzer-Alarm) branch | Builds on project 1 and sounds a buzzer when the temperature drifts away from a baseline |

---

## Hardware

- NUCLEO-F446RE (STM32F446RE)
- BME280 breakout module (BMP280 also works, without humidity)
- Passive buzzer (project 2 only)
- Jumper wires, USB cable (ST-Link provides power and the virtual COM port)

## Common CubeMX configuration

| Peripheral | Setting |
|---|---|
| I2C1 | I2C mode, Standard Mode (100 kHz), SCL = **PB6**, SDA = **PB7** |
| USART2 | Asynchronous, **115200** baud, 8N1 (ST-Link virtual COM port, PA2/PA3) |
| SYS | Debug: **Serial Wire** |
| Clock | Default 16 MHz HSI |

## Sensor wiring (both projects)

| BME280 | NUCLEO-F446RE |
|---|---|
| VCC / VIN | 3.3V |
| GND | GND |
| SCL | PB6 (Arduino header D10) |
| SDA | PB7 (Morpho connector CN7, pin 21) |

The sensor's I²C address is `0x76` when the SDO pin is tied to GND (the default on most modules) and `0x77` when it is tied to VCC. Change `BME_ADDR` in `main.c` if your module uses `0x77`.

---

## Project 1 – BME280 Sensor Reader (`main` branch)

### How it works

```
BME280 --I2C--> STM32 --UART--> Serial terminal
```

1. On start-up the firmware reads the chip ID register to detect the sensor (`0x60` = BME280, `0x58` = BMP280) and prints it.
2. It reads the factory calibration coefficients from the sensor.
3. The sensor is configured for continuous (normal) mode with 1x oversampling.
4. Every second the raw measurement is read, converted with the Bosch integer compensation formulas, and printed.

All math uses integers only, so no floating-point `printf` support is required.

### Example output

```
Sensor ID: 0x60 (BME280)
T: 28.00 C | P: 995.23 hPa | H: 64 %
T: 27.99 C | P: 995.29 hPa | H: 64 %
```

### Run it

1. Open the project in STM32CubeIDE and build it.
2. Flash the board and open a serial terminal on the ST-Link virtual COM port (115200, 8N1).
3. Press the black RESET button to see the start-up messages.

---

## Project 2 – BME280 with Buzzer Alarm (`BME280-with-Buzzer-Alarm` branch)

This branch extends project 1 with a buzzer alarm. Instead of using a fixed temperature limit, the alarm reacts to a **change** in temperature relative to a baseline measured at start-up. This makes it easy to test: just warm the sensor with your hand.

### How it works

```
Start-up: average 8 readings -> baseline temperature
Every 250 ms: measure -> compare with baseline
   |difference| >= ALARM_DELTA  -> buzzer ON
   |difference| <= ALARM_OFF    -> buzzer OFF
```

- **Baseline:** the average of 8 readings taken right after start-up (about 2 seconds).
- **Alarm on:** when the temperature differs from the baseline by `ALARM_DELTA` or more (default `300` = 3.00 °C, in either direction).
- **Alarm off:** when the difference drops to `ALARM_OFF` or below. This hysteresis value must stay lower than `ALARM_DELTA` (for example `280`), otherwise the buzzer would chatter around the threshold because of measurement noise.
- **Reset button:** pressing the blue user button (B1, PC13) sets the current temperature as the new baseline and silences the alarm.

Temperatures are handled in units of 0.01 °C (so `300` means 3.00 °C).

### Additional wiring

| Passive buzzer | NUCLEO-F446RE |
|---|---|
| + (long leg) | PA1 (Arduino header A1) |
| − | GND |

### Additional CubeMX configuration

The buzzer is a **passive** buzzer, so it needs a PWM signal to make sound:

| Peripheral | Setting |
|---|---|
| TIM2 Channel 2 | PWM Generation CH2 on **PA1** |
| Prescaler | 15 |
| Counter Period | 499 (about 2 kHz tone) |
| Pulse | 250 (50 % duty cycle) |

TIM2 is used instead of TIM4 because TIM4 CH1 shares PB6 with the I²C SCL line. The user button pin (PC13) is configured in code (`GPIOC` clock enable + input mode) rather than in the `.ioc` file.

### Example output

```
Baslangic sicakligi: 25.40 C
T: 25.41 C | Fark: +0.01 C | normal
T: 28.52 C | Fark: +3.12 C | ALARM
```

### Tuning

| Define | Meaning | Default |
|---|---|---|
| `ALARM_DELTA` | Difference that starts the alarm (0.01 °C units) | `300` (3.00 °C) |
| `ALARM_OFF` | Difference that stops the alarm (hysteresis) | below `ALARM_DELTA` |

The sensor's standby time is set to 250 ms so the alarm reacts quickly, and the main loop runs at the same rate.

---

## Troubleshooting

- **No output in the terminal:** check the COM port number and 115200 baud, make sure no other program is holding the port, and press RESET after opening the terminal. If you started a debug session, press Resume.
- **"Sensor bulunamadi" (sensor not found):** check the wiring against the `.ioc` pins (PB6/PB7), the 3.3V supply, and try address `0x77`. An I²C scan (`HAL_I2C_IsDeviceReady` over addresses 1–127) is a quick way to find the sensor.
- **Garbage first reading:** the very first sample can be incomplete if it is read immediately after configuring the sensor. Add a short `HAL_Delay(100)` after the configuration writes.
- **Humidity looks wrong or is missing:** BMP280 modules have no humidity sensor. The firmware detects this via the chip ID and skips humidity.
- **Alarm never stops:** the buzzer only stops when the difference falls to `ALARM_OFF` or below. Press the blue button to set a new baseline.

## Part of a learning path

These projects are steps in a component-based STM32 learning path (I²C level). Next steps in the path include a power monitor (INA219) and an IMU (MPU6050) on the same I²C bus.
