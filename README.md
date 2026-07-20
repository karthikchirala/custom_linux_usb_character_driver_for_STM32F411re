# Development of a Custom Linux USB Character Driver for NUCLEO-F411RE

## Project Overview

Embedded systems generally use USB-connected embedded devices for monitoring, data acquisition, and device control. In this project, the STM32F411RE performs data acquisition from connected sensors and executes commands received from the host. A custom Linux USB character driver is developed on Raspberry Pi 5 to provide a dedicated interface between Linux user-space applications and the STM32F411RE over USB. The driver implements standard character device operations to support command transmission, data retrieval, and device control through a user-space application.
