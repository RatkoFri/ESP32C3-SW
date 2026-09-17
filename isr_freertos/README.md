# isr_freertos

Builds on [`freertos_blink`](../freertos_blink) by replacing the polled
button task with a real GPIO interrupt. The two LED blink tasks are
unchanged; only the button handling logic is different.

## Hardware

Same as `freertos_blink`: yellow LED on `GPIO7`, green LED on `GPIO6`
(both active-low), button on `GPIO10` (active-low, internal pull-up).

## What it demonstrates

- Configuring a GPIO interrupt (`GPIO_INTR_NEGEDGE` — fires on the falling
  edge, i.e. the moment the button is pressed, since it's active-low).
- Writing an ISR (`IRAM_ATTR` function) that does the minimum possible work,
  since interrupt handlers run with interrupts disabled and must return
  quickly.
- A simple time-based software debounce, done directly inside the ISR.
- Using `esp_rom_printf` instead of `printf` for logging from interrupt
  context — regular `printf`/`ESP_LOG` are not safe to call from an ISR
  (they can allocate memory or block), while `esp_rom_printf` is a minimal,
  IRAM-resident printf that is.

## Code walkthrough

The two blink tasks (`utripanje_task` + `rumena_cfg`/`zelena_cfg`) are
identical to `freertos_blink` — see that README for details.

The button handling is now entirely interrupt-driven:

```c
static void IRAM_ATTR tipka_isr_handler(void *arg)
{
    int64_t zdaj = esp_timer_get_time();

    if (zdaj - zadnji_pritisk_us < DEBOUNCE_US) {
        return;                       // ignore bounces within 30ms
    }
    zadnji_pritisk_us = zdaj;

    stevec_pritiskov++;
    esp_rom_printf("ISR: pritisk tipke, stevec = %lu\n", (unsigned long)stevec_pritiskov);
}
```

`stevec_pritiskov` (press counter) is `volatile` because it's written from
interrupt context and read from normal code — without `volatile` the
compiler would be free to cache stale reads/writes across that boundary.

The debounce works by tracking the timestamp of the last *accepted* press
(`zadnji_pritisk_us`) via `esp_timer_get_time()` (microseconds since boot).
A mechanical button contact can bounce and re-trigger the interrupt several
times within a few milliseconds of a single physical press; if a new
interrupt fires less than `DEBOUNCE_US` (30ms) after the last accepted one,
it's treated as bounce noise and ignored.

`app_main` wires it up with:

```c
gpio_install_isr_service(0);
gpio_isr_handler_add(BTN_USER, tipka_isr_handler, NULL);
```

`gpio_install_isr_service` installs a shared ISR dispatcher for GPIO
interrupts (once per app); `gpio_isr_handler_add` then registers our
handler for the specific button pin.

## Why this is better than polling

The interrupt fires the instant the button changes state, so there's no
150ms polling latency and no risk of missing a very short press between
polls — the hardware notifies the CPU immediately, regardless of what else
is running.

## Using this example

Same as `freertos_blink`: copy this file into an ESP-IDF project's `main/`
folder as its main source file. No other project scaffolding is included
here.
