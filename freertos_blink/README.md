# freertos_blink

The starting point of this series: two LEDs blinking independently and a
button being polled for its state, all built with FreeRTOS tasks instead of
a single sequential loop. This is the base the other three examples build on.

## Hardware

- Yellow LED on `GPIO7`
- Green LED on `GPIO6`
- Both LEDs are wired common-anode style: the GPIO drives the *cathode*, so
  `0` = LED on, `1` = LED off (see the `LED_ON` / `LED_OFF` defines).
- User button on `GPIO10`, wired to ground when pressed (active-low), with
  the internal pull-up enabled.

## What it demonstrates

- Running multiple independent pieces of logic concurrently on top of
  FreeRTOS, instead of interleaving everything by hand in one `while(1)`
  loop in `app_main`.
- Writing one *generic* task function and running it multiple times with
  different parameters, rather than duplicating the same loop per LED.
- Polling a GPIO input from a task (the simplest, if not most efficient, way
  to react to a button).

## Code walkthrough

**`utripanje_cfg_t`** is a small struct — pin, a name for logging, and a
blink period — passed as the task argument. Because both LEDs need the same
behavior (toggle, log, delay) just with different pins/periods, one task
function (`utripanje_task`) is created twice with two different configs
(`rumena_cfg` for yellow, `zelena_cfg` for green) rather than writing two
near-identical functions.

```c
xTaskCreate(utripanje_task, "utripanje_rumena", 2048, &rumena_cfg, 5, NULL);
xTaskCreate(utripanje_task, "utripanje_zelena", 2048, &zelena_cfg, 5, NULL);
```

Each task just flips its LED, prints the new state, and calls
`vTaskDelay()` for its own period — the `vTaskDelay` call is what lets the
two blink tasks (and the button task) run concurrently: it yields the CPU
back to the scheduler instead of busy-waiting.

**`tipka_task`** is a third task that polls the button every 150ms:

```c
bool pritisnjena = (gpio_get_level(BTN_USER) == 0);
printf("tipka: %s\n", pritisnjena ? "DA" : "NE");
```

Polling on a timer is simple to reason about but has an inherent downside:
a very short button press can be missed if it doesn't overlap with a poll,
and even when it isn't missed there's up to 150ms of latency before it's
noticed. That limitation is exactly what the next example (`isr_freertos`)
fixes by switching to a hardware interrupt.

**`app_main`** just does the GPIO setup for both LEDs (as outputs) and the
button (as an input with pull-up), sets the initial LED state to off, and
creates the three tasks. It returns immediately afterward — the tasks keep
running on their own.

## Using this example

This file is a drop-in replacement for `main/<project>_main.c` in an
ESP-IDF project targeting the ESP32-C3 (or any target — just adjust the
`LED_GREEN`/`LED_YELLOW`/`BTN_USER` GPIO numbers for your board's wiring).
It has no other project files here because it's meant to be copied into an
existing ESP-IDF project's `main/` folder, not built standalone.
