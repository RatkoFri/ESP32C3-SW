# isr_no_freertos

A variant of [`isr_freertos`](../isr_freertos) that avoids calling any
FreeRTOS API in application code — no `xTaskCreate`, no `vTaskDelay`, no
`TaskHandle_t`. Both LEDs are driven from a single sequential loop in
`app_main` instead of separate tasks, timed with a hardware timer instead
of the scheduler.

## An important caveat, up front

**ESP-IDF itself is built on top of FreeRTOS.** Even here, `app_main` is
still, structurally, a FreeRTOS task — there is no supported way to strip
that out without leaving the ESP-IDF framework entirely (writing bare-metal
directly against the chip's ROM/HAL, which is a much bigger undertaking).
What this example actually achieves is containing **zero explicit FreeRTOS
calls in the application code itself** — a useful exercise in seeing what
that costs you, not a literal FreeRTOS-free firmware.

## Hardware

Same as the other examples: yellow LED on `GPIO7`, green LED on `GPIO6`
(both active-low), button on `GPIO10` (active-low, internal pull-up).

## What it demonstrates

- Non-blocking timing using `esp_timer_get_time()` timestamp comparisons
  instead of `vTaskDelay` — the classic "Arduino `millis()`-style" pattern
  for scheduling periodic work inside a single loop.
- `esp_rom_delay_us()` as a CPU busy-wait, in place of a scheduler-yielding
  delay.
- Why removing all yields has a real cost: the **task watchdog timer**
  monitors the idle task, and if the CPU never yields to it, the watchdog
  will reset the board after a few seconds. `esp_task_wdt_deinit()` is
  called at startup specifically to avoid that, because this loop's
  `esp_rom_delay_us()` call never yields.
- A header-inclusion gotcha: `IRAM_ATTR` normally comes in transitively via
  `freertos/portmacro.h` (included by `freertos/FreeRTOS.h`). Once those
  FreeRTOS includes are removed, `IRAM_ATTR` has to be pulled in directly
  via `esp_attr.h`, or the build fails with an undeclared macro.

## Code walkthrough

The button ISR and its debounce logic (`tipka_isr_handler`,
`zadnji_pritisk_us`, `DEBOUNCE_US`) are unchanged from `isr_freertos` — see
that README for the details on why the debounce works the way it does.

The blinking logic is the main difference. Instead of two tasks each
sleeping for their own period, `app_main` tracks the next toggle time for
each LED and checks both on every loop iteration:

```c
int64_t naslednji_rumena_us = esp_timer_get_time();
int64_t naslednji_zelena_us = esp_timer_get_time();

while (1) {
    int64_t zdaj = esp_timer_get_time();

    if (zdaj >= naslednji_rumena_us) {
        stanje_rumena = !stanje_rumena;
        gpio_set_level(LED_YELLOW, stanje_rumena ? LED_ON : LED_OFF);
        naslednji_rumena_us += PERIODA_RUMENA_US;
    }

    if (zdaj >= naslednji_zelena_us) {
        stanje_zelena = !stanje_zelena;
        gpio_set_level(LED_GREEN, stanje_zelena ? LED_ON : LED_OFF);
        naslednji_zelena_us += PERIODA_ZELENA_US;
    }

    esp_rom_delay_us(1000);
}
```

Both LEDs are handled in the same loop iteration rather than needing
separate tasks, because each one just checks "has my period elapsed?"
independently — this is the fundamental trade-off of cooperative,
single-loop scheduling versus preemptive tasks: it works fine as long as no
single piece of work blocks for long, but doesn't scale as cleanly if one
LED's logic needed to block (e.g. waiting on I/O) without stalling the
other.

At the very top of `app_main`:

```c
esp_task_wdt_deinit();
```

This has to run before the loop starts, precisely because the loop's
`esp_rom_delay_us(1000)` busy-waits the CPU rather than yielding it — see
the caveat above.

## Using this example

Copy this file into an ESP-IDF project's `main/` folder as its main source
file, same as the other examples. No other project scaffolding is included
here.
