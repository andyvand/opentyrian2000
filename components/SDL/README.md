# Simple Direct Layer - SDL3 (ESP-IDF port)

This is component wrapper for upstream SDL3 for ESP-IDF.

The HW layer is using [ESP-BSP](https://components.espressif.com/components?q=tags:bsp).

In the project CMakeLists.txt it's necessary to set `ENV{BUILD_BOARD}` to name of BSP.

```
set(ENV{BUILD_BOARD} ${BUILD_BOARD})
```

## Examples

### C Examples

- https://github.com/georgik/esp32-sdl3-example
- https://github.com/georgik/OpenTyrian
- https://github.com/georgik/esp32-weather-display

### Embedded Swift Examples

- https://github.com/georgik/esp32-sdl3-swift-example/
- https://github.com/georgik/esp32-swift-weather-display

## Supported

Support is achieved by using [ESP-BSP](https://github.com/espressif/esp-bsp)

- ESP32-C3-LcdKit
- ESP32-C6-DevKit
- ESP32-S3-BOX-3
- ESP32-P4 Evaluation Kit
  - with PPA for scaling the screen
- M5Stack CoreS3


