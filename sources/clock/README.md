# Nixie Clock — code

The clock's C++ sources. **Documentation for the whole project — hardware,
SD card build, configuration, deployment — is in the
[root README](../../README.md).**

## Targets

| Target | What it is |
|---|---|
| `nixie` | The main process: hardware, FSM, time, weather forecast |
| `camera` | Face and motion detection; notifies `nixie` by signal |
| `nixie.cgi` | Backend for the web page forms |
| `logger` | `liblogger.so`, the log shared by all three |

## Building

The Pi Zero is ARMv6, and a Debian/Ubuntu armhf toolchain produces ARMv7
binaries that do not run on it. So the build happens inside a Raspberry Pi OS
Bullseye armhf container, emulated through `qemu-arm`:

    ./docker/build.sh                  # all targets
    ./docker/build.sh nixie camera     # just some
    ./docker/build.sh --shell          # a shell in the container
    ./docker/build.sh --clean          # discard the build directory

Output goes to `dockerbuild/`. The `build/` and `rpibuild/` folders hold CMake
caches made on the Pi itself and cannot be reused on the PC.

Host prerequisites and the install flow on the clock are in the
[root README](../../README.md#development).

## Layout

    src/nixie/        main process
      hardware/       tubes, PWM, bargraph, LEDs, dimmer, GPIO
      modulation/     sinusoidal, flash, ramp
    src/camera/       capture, face detection, motion detection
    src/cgi-bin/      the web page CGI
    src/logger/       liblogger.so (bundled spdlog)
    src/utils/        cJSON, config parser, signals, timers, median

## Code documentation

Doxygen, from the `Doxyfile`:

    doxygen Doxyfile      # output in docs/html/, open index.html

## Logs

The clock writes to `/tmp/nixie.txt`, also visible in the site's *Logs* tab.

## Author

[Luiz Cressoni](mailto:luiz@cressoni.com.br)
