# FRAMOS Sensor Module drivers for Raspberry Pi

Getting started with [Framos General Optics modules FSM:GO](https://www.framos.com/en/fsmgo?utm_source=google&utm_medium=cpc&utm_campaign=FSM-GO_Product_Launch_2024) and Raspberry Pi.

This repository contains driver-source installation instructions for FRAMOS General Optic modules FSM:GO
#
> [!TIP]
> Read official [Raspberry Pi documentation](https://www.raspberrypi.com/documentation/) for better understanding. Important chapters to highlight from Raspberry Pi documentation are:
> - [Install an operating system](https://www.raspberrypi.com/documentation/computers/getting-started.html#installing-the-operating-system)
> - [The Linux kernel](https://www.raspberrypi.com/documentation/computers/linux_kernel.html)
> - [Camera software](https://www.raspberrypi.com/documentation/computers/camera_software.html)
#
## Supported Raspberry Pi models

- Raspberry Pi 5

## Supported Raspberry Pi OS versions and Framos branch compatibility

This list describes which "Linux source tag" and "framos-rpi-drivers branch" to use for the desired Raspberry Pi OS release.

If using cross-compilation, the "Linux source" tag is used to checkout to correct tag of the Raspberry Pi kernel source code and the "framos-rpi-drivers branch" to checkout to compatible Framos drivers source code.

If using target build, the "framos-rpi-drivers branch" is used to checkout to compatible Framos drivers source code for the desired Raspberry Pi OS.

Click "Download" to automatically start downloading Raspberry Pi OS Image.

|Raspberry Pi OS|Linux source tag|framos-rpi-drivers branch|Release notes|
|-|-|-|-|
|[Raspberry Pi OS (64-bit) 2024-11-19](https://downloads.raspberrypi.com/raspios_arm64/images/raspios_arm64-2024-11-19/) [[Download]](https://downloads.raspberrypi.com/raspios_arm64/images/raspios_arm64-2024-11-19/2024-11-19-raspios-bookworm-arm64.img.xz)|stable_20241008|framos_20241008|[framos_20241008](https://github.com/framosimaging/framos-rpi-drivers/wiki/Release%E2%80%90Notes%E2%80%90framos_20241008)|


# Short procedure

## 1. Install Raspberry Pi OS

- [Raspberry Pi OS installation guide](https://github.com/framosimaging/framos-rpi-drivers/wiki/Install%E2%80%90Raspberry%E2%80%90Pi%E2%80%90OS%E2%80%90(64%E2%80%90bit))

## 2. Get & Install Framos drivers

Two methods:
- [Compile Framos source code on target system(Raspberry Pi)](https://github.com/framosimaging/framos-rpi-drivers/wiki/FRAMOS%E2%80%90Sensor%E2%80%90Module%E2%80%90drivers-%E2%80%90-Target%E2%80%90build)

or

- [Cross-compile Framos source code on host system(Ubuntu 22.04)](https://github.com/framosimaging/framos-rpi-drivers/wiki/FRAMOS%E2%80%90Sensor%E2%80%90Module%E2%80%90drivers-%E2%80%90-Cross%E2%80%90compile)

## 3. Get & Install Framos-libcamera

- [Proceed to framos-libcamera rpository](https://github.com/framosimaging/framos-libcamera)
