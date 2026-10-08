# stm32f7_dashboard

Zephyr RTOS application for the **STM32F7508-DK**, with an LVGL touch UI
covering system diagnostics, a PC-sampling CPU profiler, and a
tilt-controlled game driven by an MPU6050 accelerometer/gyroscope.

## Hardware

- **Board**: STM32F7508-DK (480x272 LCD, capacitive touch)
- **Sensor**: MPU6050 (accel/gyro) on I2C1, address `0x68` — see [wiring](#mpu6050-wiring)
- Runs XIP from external QSPI flash via a two-stage boot chain
  (`boot_qspi` chain-loads `stm32f7_dashboard`)

## Prerequisites

- A working Zephyr development environment (`west`, SDK, toolchain) —
  see the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
- OpenOCD for flashing

## Build & Flash

Two independent apps in this workspace, `boot_qspi` and `stm32f7_dashboard`,
built and flashed separately:

```sh
cd boot_qspi
west build -b stm32f7508_dk .
west flash --runner openocd

cd ../stm32f7_dashboard
west build -b stm32f7508_dk .
west flash --runner openocd
```

`stm32f7_dashboard` needs **two consecutive builds** after any source change: the
first regenerates the CPU profiler's symbol table from the freshly
linked ELF, the second bakes it into the binary.

```sh
west build -b stm32f7508_dk .
west build -b stm32f7508_dk .
```

## MPU6050 Wiring

| MPU6050 | STM32F7508-DK (Arduino connector) |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | D15 (PB8, I2C1_SCL) |
| SDA | D14 (PB9, I2C1_SDA) |
| AD0 | GND (sets address to `0x68`) |

## Screens

| Screen | Description |
|---|---|
| System Overview | Board info, die temperature/Vref, uptime, PC-synced clock |
| CPU Profiler | PC-sampling profiler table (function, % CPU, samples, address, size) |
| Event Logs | Circular buffer of timestamped log lines |
| Live Variables | LVGL heap and thread stack diagnostics |
| Ball Game | Tilt-controlled bouncing ball vs 2 chasing obstacles; game over with score, persistent best score, Continue/Menu |

Also included: an IWDG hardware watchdog with boot-time reset-cause
reporting, and a PC time-sync link over UART.

The ball game's best score survives reboots: it is stored in a dedicated
32 KB partition in internal flash (`highscore_partition` at offset
`0x8000`, below the `boot_qspi` bootloader's 23.5 KB image). `west flash`
of this app never touches it — the generated image targets the QSPI XIP
window (`0x90000000`) only.

## License

MIT — see [LICENSE](LICENSE).
