#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

// Minimalna BLE "beacon" demonstracija: naprava samo oglasuje (advertise),
// brez GATT streznika in brez povezovanja (pairing). Stevec pritiskov tipke
// je vkljucen v oglasevalne podatke (manufacturer data) - vsak telefon z
// BLE skenerjem (npr. nRF Connect) ga lahko vidi, ne da bi se sploh povezal.

#define TAG          "ble_broadcast"
#define DEVICE_NAME  "ESP32C3-tipka"

// ni deklarirana v store/config/ble_store_config.h, zato deklaracija tu
// (enako pocne uradni ESP-IDF primer NimBLE_Beacon)
void ble_store_config_init(void);

#define BTN_USER     GPIO_NUM_10
#define DEBOUNCE_US  30000   // 30 ms

static uint8_t own_addr_type;

// Stevec pritiskov tipke - posodablja ga naloga, sprozena preko ISR-ja
static volatile uint32_t stevec_pritiskov = 0;
static volatile int64_t zadnji_pritisk_us = 0;

static TaskHandle_t tipka_task_handle = NULL;

static void posodobi_oglasevanje(void);

// ISR se sprozi ob padajocem robu (pritisk tipke, ker je aktivna nizko)
// V ISR-ju ne smemo klicati NimBLE funkcij - samo obvestimo cakajoco nalogo
static void IRAM_ATTR tipka_isr_handler(void *arg)
{
    int64_t zdaj = esp_timer_get_time();

    // programski debounce - prehitre ponovne prekinitve (odboj kontaktov) ignoriramo
    if (zdaj - zadnji_pritisk_us < DEBOUNCE_US) {
        return;
    }
    zadnji_pritisk_us = zdaj;
    stevec_pritiskov++;

    BaseType_t visja_prioriteta_je_zbujena = pdFALSE;
    vTaskNotifyGiveFromISR(tipka_task_handle, &visja_prioriteta_je_zbujena);
    portYIELD_FROM_ISR(visja_prioriteta_je_zbujena);
}

// Naloga caka na notifikacijo iz ISR-ja in nato (v varnem, ne-ISR kontekstu)
// posodobi oglasevalne podatke z novo vrednostjo stevca
static void tipka_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "pritisk tipke, stevec = %lu", (unsigned long)stevec_pritiskov);
        posodobi_oglasevanje();
    }
}

// Sestavi oglasevalne podatke (ime + stevec v manufacturer data) in
// (ponovno) zazene oglasevanje - klice se ob zagonu in ob vsakem pritisku
static void posodobi_oglasevanje(void)
{
    struct ble_hs_adv_fields fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    // Manufacturer specific data: 2 bajta "company id" (0xFFFF = rezervirano
    // za testiranje/interno rabo, ni za prave izdelke) + 4 bajti stevca
    static uint8_t mfg_data[6];
    uint32_t stevec = stevec_pritiskov;
    mfg_data[0] = 0xFF;
    mfg_data[1] = 0xFF;
    mfg_data[2] = (uint8_t)(stevec       & 0xFF);
    mfg_data[3] = (uint8_t)((stevec >> 8)  & 0xFF);
    mfg_data[4] = (uint8_t)((stevec >> 16) & 0xFF);
    mfg_data[5] = (uint8_t)((stevec >> 24) & 0xFF);

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.mfg_data = mfg_data;
    fields.mfg_data_len = sizeof(mfg_data);

    // ce ze oglasujemo, moramo najprej ustaviti, da lahko spremenimo podatke
    ble_gap_adv_stop();

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "napaka pri nastavljanju oglasevalnih podatkov: %d", rc);
        return;
    }

    // ne-povezljiv, splosno-zaznaven nacin -> pravi "beacon"
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // hiter interval oglasevanja (30-60 ms) namesto privzetega pocasnega (~1s),
    // da skener stevec zazna skoraj takoj po pritisku
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "napaka pri zagonu oglasevanja: %d", rc);
    }
}

// Poklicano, ko se NimBLE gostiteljski sklad uspesno sinhronizira s krmilnikom
static void on_stack_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &own_addr_type);
    posodobi_oglasevanje();
}

static void on_stack_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE sklad se je ponastavil, razlog: %d", reason);
}

static void nimble_host_task(void *param)
{
    // funkcija se vrne sele, ko se pokliče nimble_port_stop()
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // konfiguracija GPIO-ja za tipko - enako kot v prejsnjih primerih
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_USER),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);

    xTaskCreate(tipka_task, "tipka", 4096, NULL, 5, &tipka_task_handle);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_USER, tipka_isr_handler, NULL);

    ESP_ERROR_CHECK(nimble_port_init());

    ble_svc_gap_init();
    ble_svc_gap_device_name_set(DEVICE_NAME);

    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb  = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    xTaskCreate(nimble_host_task, "nimble_host", 4096, NULL, 5, NULL);
}
