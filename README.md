# ESP32C3-SW

> **Workshop material.** These examples build on
> [FRI-ESP32-C3-MINI-blinky](https://github.com/bulicp/FRI-ESP32-C3-MINI-blinky),
> a reference example from the Faculty of Computer and Information Science
> (University of Ljubljana). Contributed as part of the skills and
> education program of **CC Chip.si**, the Slovenian Competence Center on
> Chips and Semiconductor Technologies ([cc-chip.si](https://cc-chip.si/about/)).

A small progression of ESP32-C3 examples, each building on the previous
one — starting from a single-loop baseline and working up through FreeRTOS
tasks, interrupts, and finally a BLE beacon. All share the same base
hardware: two LEDs (yellow on `GPIO7`, green on `GPIO6`, both active-low)
and a user button (`GPIO10`, active-low).

## Requirements

- An ESP32-C3 board (e.g. an ESP32-C3-MINI-1 dev board), USB cable, and
  the two LEDs + button wired as described above (or adjust the GPIO
  numbers at the top of each source file to match your own wiring).
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/get-started/index.html)
  (the examples here were built/tested against ESP-IDF v5.x on the
  ESP32-C3 target).
- A serial terminal (`idf.py monitor`, or any USB-serial terminal) to see
  the `printf`/`ESP_LOG` output from each example.
- For `ble_broadcast` only: a BLE scanner to observe the advertisement —
  a phone app (nRF Connect, LightBlue, ...), or `bluetoothctl`/`bleak` on
  Linux.

## Tools

- **ESP-IDF** (`idf.py`) for building and flashing — either the command
  line tools, or the [ESP-IDF VS Code extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).
- **A C compiler toolchain for RISC-V** (ESP32-C3 is a RISC-V target) —
  installed automatically by the ESP-IDF install script.
- Optional: [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile)
  or similar, for inspecting the `ble_broadcast` advertisement from a
  phone.

## Examples

| Example | What it adds |
|---|---|
| [`blinky_baseline`](blinky_baseline) | The original reference example: everything in one sequential loop, no extra tasks, no interrupts. |
| [`freertos_blink`](freertos_blink) | Two LEDs blinking via independent FreeRTOS tasks; button read by polling. |
| [`isr_freertos`](isr_freertos) | Same LEDs, but the button is now interrupt-driven (with debounce) instead of polled. |
| [`isr_no_freertos`](isr_no_freertos) | Same behavior as `isr_freertos`, but with zero explicit FreeRTOS calls in the application code — single loop, timer-based scheduling. |
| [`ble_broadcast`](ble_broadcast) | A standalone project: the button press count is broadcast over BLE as a beacon, visible to any scanner with no pairing required. |

Each folder has its own README with a full code walkthrough. All but
`ble_broadcast` are single `.c` files meant to be copied into an existing
ESP-IDF project's `main/` folder; `ble_broadcast` is a complete,
independently buildable ESP-IDF project (it needs Bluetooth-related
build/Kconfig changes that don't fit into an arbitrary existing project).

## Contributing

This is workshop material for CC Chip.si — if you spot a bug, have a
clearer way to explain a concept, or want to add another step to the
progression, contributions are welcome via issues or pull requests.
