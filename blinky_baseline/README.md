# blinky_baseline

The actual starting point of this whole series — sourced from
[FRI-ESP32-C3-MINI-blinky](https://github.com/bulicp/FRI-ESP32-C3-MINI-blinky),
the course's reference example. Everything else in this repo
(`freertos_blink`, `isr_freertos`, `isr_no_freertos`, `ble_broadcast`) is a
progressive rewrite of this one file.

## Hardware

- Yellow LED on `GPIO7`, green LED on `GPIO6` — both active-low (the GPIO
  drives the LED's cathode, so `0` = on, `1` = off).
- User button on `GPIO10`, active-low, internal pull-up enabled.

## What it demonstrates

This is deliberately the *simplest possible* version: no extra FreeRTOS
tasks, no interrupts — just `app_main` itself as a single loop that does
everything in sequence, once every 150ms:

```c
while (1) {
    stanje = !stanje;
    gpio_set_level(LED_YELLOW, stanje ? LED_ON : LED_OFF);

    bool pritisnjena = (gpio_get_level(BTN_USER) == 0);
    gpio_set_level(LED_GREEN, pritisnjena ? LED_ON : LED_OFF);

    printf("utrip: %d, tipka: %s\n", stanje, pritisnjena ? "DA" : "NE");

    vTaskDelay(pdMS_TO_TICKS(150));
}
```

Every 150ms it flips the yellow LED's state, reads the button and mirrors
it onto the green LED (on only while held), logs both, then sleeps. Note
that `app_main` itself technically already runs as a FreeRTOS task under
ESP-IDF (that's unavoidable — see `isr_no_freertos`'s README for more on
this), but no *additional* tasks are created here; there's exactly one
thread of control doing everything.

## Where this falls short (and what the other examples fix)

- **Green LED tracks the button only while this exact loop iteration runs**
  — a very short press between two 150ms iterations can be missed
  entirely, and even a press that *is* seen has up to 150ms of latency.
  [`isr_freertos`](../isr_freertos) fixes this with a real GPIO interrupt.
- **Both LEDs are forced to blink at the same rate**, because they're
  updated from the same loop with a single shared delay. Giving each LED
  its own independent timing (as in [`freertos_blink`](../freertos_blink))
  needs either separate tasks or explicit per-LED timestamp tracking (as
  in [`isr_no_freertos`](../isr_no_freertos)).
- There's no way to observe the button count remotely — that's what
  [`ble_broadcast`](../ble_broadcast) adds.

## Using this example

Copy this file into an ESP-IDF project's `main/` folder as its main source
file. No other project scaffolding is included here.
