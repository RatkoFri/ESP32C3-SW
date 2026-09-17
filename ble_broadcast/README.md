# ble_broadcast

A minimal BLE **beacon**: the board advertises its button-press counter over
Bluetooth Low Energy, with no GATT server and no pairing/connection at all.
Any phone with a generic BLE scanner app (nRF Connect, LightBlue, ...) — or
a scanning script on a laptop — can read the counter just by seeing the
advertisement, without ever connecting to the device.

Unlike the other three examples in this repo, this one is a complete,
standalone ESP-IDF project (it has its own `CMakeLists.txt`,
`sdkconfig.defaults`, and `main/`), because bringing up Bluetooth requires
real build-system changes (linking the `bt` component, enabling NimBLE in
Kconfig) that can't just be pasted into an arbitrary project's `main.c`.

## Hardware

Button on `GPIO10` (active-low, internal pull-up). No LEDs are used in this
example — it's purely about the button-to-Bluetooth path.

## Why a beacon, and not a GATT server?

A GATT server (services/characteristics, connect from a phone, read/write
values) is the more "interactive" way to do this over BLE, but it comes
with a lot more boilerplate: UUIDs, characteristic callbacks, connection
handling. A beacon is the deliberately minimal option — one-directional,
no pairing, visible to literally any scanner — chosen here to keep the
example focused on "get a value from the button onto the air" rather than
on GATT plumbing.

## Building and flashing

```
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

`sdkconfig.defaults` enables Bluetooth (`CONFIG_BT_ENABLED`) and the
NimBLE host (`CONFIG_BT_NIMBLE_ENABLED`) — these are off by default in a
fresh ESP-IDF project, so a project that adds Bluetooth to existing code
needs this even if the button/GPIO parts already worked before.

## Code walkthrough

### Button → counter (interrupt-driven, same idea as `isr_freertos`)

The button is still handled via a GPIO interrupt with a software debounce,
exactly like in [`isr_freertos`](../isr_freertos) — see that README for why
the debounce works the way it does. The one difference: **NimBLE functions
are not safe to call from interrupt context**, so the ISR can't directly
update the advertisement. Instead it just increments the counter and wakes
a waiting task:

```c
static void IRAM_ATTR tipka_isr_handler(void *arg)
{
    ...
    stevec_pritiskov++;
    vTaskNotifyGiveFromISR(tipka_task_handle, &visja_prioriteta_je_zbujena);
    portYIELD_FROM_ISR(visja_prioriteta_je_zbujena);
}

static void tipka_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        posodobi_oglasevanje();   // safe here: normal task context, not an ISR
    }
}
```

### Building the advertisement (`posodobi_oglasevanje`)

The counter is packed into the advertisement's **manufacturer specific
data** field: 2 bytes for a company ID, followed by the payload.

```c
mfg_data[0] = 0xFF;
mfg_data[1] = 0xFF;                              // 0xFFFF = reserved for testing, not a real company ID
mfg_data[2] = (uint8_t)(stevec        & 0xFF);   // counter, little-endian
mfg_data[3] = (uint8_t)((stevec >> 8)  & 0xFF);
mfg_data[4] = (uint8_t)((stevec >> 16) & 0xFF);
mfg_data[5] = (uint8_t)((stevec >> 24) & 0xFF);
```

`0xFFFF` is the Bluetooth SIG's reserved "testing only" company ID — fine
for a lab demo, but a real product would register/use its own assigned ID.

Since advertisement data can't be changed while advertising is already
running, updating it means: stop, set new fields, start again:

```c
ble_gap_adv_stop();
ble_gap_adv_set_fields(&fields);
...
ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
```

`adv_params.conn_mode = BLE_GAP_CONN_MODE_NON` and `disc_mode =
BLE_GAP_DISC_MODE_GEN` are what make this a true beacon: non-connectable,
generally discoverable.

`adv_params.itvl_min`/`itvl_max` are explicitly set to
`BLE_GAP_ADV_FAST_INTERVAL1_MIN`/`MAX` (30–60ms). Left at the default (0),
NimBLE falls back to a much slower interval (~1 second), meaning a scanner
could take a noticeable moment to catch a fresh advertisement after a
press. The fast interval makes a button press visible to a nearby scanner
almost immediately.

### Bringing up the NimBLE host (`app_main`)

```c
ESP_ERROR_CHECK(nimble_port_init());

ble_svc_gap_init();
ble_svc_gap_device_name_set(DEVICE_NAME);

ble_hs_cfg.reset_cb = on_stack_reset;
ble_hs_cfg.sync_cb  = on_stack_sync;
ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
ble_store_config_init();

xTaskCreate(nimble_host_task, "nimble_host", 4096, NULL, 5, NULL);
```

`nimble_port_init()` brings up the BLE controller and host. The host then
runs its own event loop in a dedicated task (`nimble_host_task`, via
`nimble_port_run()`), and calls `on_stack_sync()` once it's ready — that's
where the first advertisement actually gets started, since advertising
can't begin before the stack has synced with the controller and figured
out its own Bluetooth address (`ble_hs_id_infer_auto`).

One header quirk worth calling out: `ble_store_config_init()` is defined in
`ble_store_config.c` but never actually declared in
`store/config/ble_store_config.h`. It needs a manual forward declaration
(see the top of the file) — ESP-IDF's own bundled `NimBLE_Beacon` example
does the same workaround, so this isn't a mistake specific to this project.

## Observing the counter

From a phone: open any BLE scanner app, find the device advertising as
`ESP32C3-tipka`, and look at its manufacturer data (4 bytes, little-endian)
— it should update within roughly the fast-advertising-interval window of
pressing the button.

From a Linux laptop, the quickest manual check is
`bluetoothctl scan on`, then `bluetoothctl info <mac>` and look at
`ManufacturerData.Value`. For a live view, use the included
[`watch_stevec.py`](watch_stevec.py) instead — it hooks directly into
BlueZ's advertisement callback via the `bleak` library, so it reacts the
instant a new packet arrives (no polling delay), and looks the device up
by name so there's no MAC address to hardcode.

### Setting up the environment for `watch_stevec.py`

On distros that enforce [PEP 668](https://peps.python.org/pep-0668/)
(e.g. Arch Linux), a plain `pip install` is blocked system-wide, so use a
virtual environment:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

Then run it (leave it running, no `scan on` needed separately — the script
manages scanning itself):

```bash
.venv/bin/python watch_stevec.py
```

It prints `iscem napravo 'ESP32C3-tipka' ...` while searching, then
`stevec pritiskov: N` every time the counter changes. No special
permissions are needed beyond normal Bluetooth access (the same access
level `bluetoothctl` already has for your user).

## A known rough edge

Repeatedly stopping and restarting advertising on every single button
press (rather than, say, batching updates or using a less disruptive
update mechanism) is the simplest approach and works, but it's worth being
aware that very rapid, repeated presses mean very rapid, repeated
stop/set/start cycles on the NimBLE host — something to watch for if you
extend this example and start seeing the host misbehave under heavy button
mashing.
