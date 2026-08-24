[README.md](https://github.com/user-attachments/files/31395212/README.md)
# N6_BME680_MPU5060

A [Zephyr RTOS](https://zephyrproject.org/) application that reads an **MPU6050** IMU (accelerometer/gyroscope) and a **BME680** environmental sensor (temperature, humidity, pressure, gas/VOC) over I2C, and displays the data on an LCD using **LVGL**. The target board uses a dedicated AXISRAM region for the graphics memory pool, indicating an STM32N6-class MCU (e.g. STM32N6570-DK or similar).

## Overview

This project combines two common I2C sensors with a graphical dashboard built in Zephyr + LVGL:

- **BME680** — temperature, humidity, barometric pressure, and gas resistance (air quality)
- **MPU6050** — 3-axis accelerometer + 3-axis gyroscope

Sensor readings are periodically polled and rendered to the display through an LVGL UI (generated with a UI tool such as SquareLine Studio, based on the `src/ui/` file layout), giving a live, on-device readout of environmental and motion data.

## Hardware

| Component | Interface | Address |
|---|---|---|
| MPU6050 (accelerometer/gyroscope) | I2C1 | `0x68` |
| BME680 (temp / humidity / pressure / gas) | I2C1 | `0x77` |
| Display (LVGL) | — | Uses `AXISRAM3` memory pool |

Both sensors are wired to the same I2C bus (`i2c1`) as configured in [`app.overlay`](./app.overlay). The MPU6050 sample rate divider is set via `smplrt-div`.

## Software / Features

- Built on **Zephyr RTOS**, using the sensor subsystem drivers for `BME680` and `MPU6050`
- **LVGL** graphics stack for the on-device UI (charts, arcs, labels)
- Shell and logging enabled for debugging (`CONFIG_SHELL`, `CONFIG_LOG`, `CONFIG_I2C_SHELL`)
- Runtime heap statistics enabled (`CONFIG_SYS_HEAP_RUNTIME_STATS`)

## Project Structure

```
.
├── CMakeLists.txt        # Build configuration and source file list
├── Kconfig                # Application-level Kconfig options
├── prj.conf                # Zephyr project configuration (drivers, LVGL, shell, etc.)
├── app.overlay             # Devicetree overlay: I2C sensors + AXISRAM3 region
├── src/
│   ├── main.c               # Application entry point / sensor polling loop
│   └── ui/
│       ├── images.c          # LVGL image assets
│       ├── screens.c         # LVGL screen definitions
│       ├── styles.c          # LVGL style definitions
│       └── ui.c               # LVGL UI initialization/glue code
├── .vscode/                # VS Code workspace settings
└── .idea/                  # JetBrains/CLion project settings
```

## Getting Started

### Prerequisites

- [Zephyr RTOS](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) development environment (SDK + `west` toolchain)
- A supported board with an available I2C bus and display, wired to a BME680 and an MPU6050 sensor as described above

### Build

```bash
west build -b <your_board> N6_BME680_MPU5060
```

### Flash

```bash
west flash
```

Replace `<your_board>` with the Zephyr board target matching your hardware (e.g. an STM32N6-based board).

## Configuration

Key options are set in [`prj.conf`](./prj.conf), including:

- `CONFIG_SENSOR`, `CONFIG_BME680`, `CONFIG_MPU6050_TRIGGER_NONE` — sensor drivers
- `CONFIG_I2C`, `CONFIG_I2C_SHELL` — I2C bus support and shell access
- `CONFIG_DISPLAY`, `CONFIG_LVGL` and related `CONFIG_LV_*` options — display and graphics stack
- `CONFIG_LV_Z_MEMORY_POOL_ZEPHYR_REGION_NAME="AXISRAM3"` — dedicated RAM region for the LVGL memory pool

## Status

This is a learning/exercise project for working with Zephyr RTOS, I2C sensor drivers, and LVGL-based UIs on embedded hardware.

## License

No license has been specified for this repository. Add a `LICENSE` file if you'd like to make the reuse terms explicit.
