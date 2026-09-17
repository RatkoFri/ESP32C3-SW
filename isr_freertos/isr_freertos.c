#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define LED_GREEN   GPIO_NUM_6
#define LED_YELLOW  GPIO_NUM_7
#define BTN_USER    GPIO_NUM_10

// LED-ici imata katodo na GPIO -> 0 = prizgano, 1 = ugasnjeno
#define LED_ON  0
#define LED_OFF 1

// Nastavitve za en utripajoc LED: kateri pin, ime (za izpis) in perioda utripanja
typedef struct {
    gpio_num_t pin;
    const char *ime;
    TickType_t perioda;
} utripanje_cfg_t;

// Vsak LED ima svojo periodo, da je razlika med njima vidna
static utripanje_cfg_t rumena_cfg = { .pin = LED_YELLOW, .ime = "rumena", .perioda = pdMS_TO_TICKS(150) };
static utripanje_cfg_t zelena_cfg = { .pin = LED_GREEN,  .ime = "zelena", .perioda = pdMS_TO_TICKS(300) };

// Generična naloga (task) za utripanje enega LED-a
// arg je kazalec na utripanje_cfg_t, zato se ista funkcija uporabi za oba LED-a
static void utripanje_task(void *arg)
{
    utripanje_cfg_t *cfg = (utripanje_cfg_t *)arg;
    int stanje = 0;

    while (1) {
        // ob vsaki iteraciji obrnemo stanje LED-a (prizgan/ugasnjen)
        stanje = !stanje;
        gpio_set_level(cfg->pin, stanje ? LED_ON : LED_OFF);
        printf("utrip %s: %d\n", cfg->ime, stanje);

        // naloga se za svojo periodo "uspava" -> ne blokira ostalih nalog
        vTaskDelay(cfg->perioda);
    }
}

// Stevec pritiskov tipke - povecuje ga ISR, zato je volatile
static volatile uint32_t stevec_pritiskov = 0;

// Cas zadnjega priznanega pritiska (za debounce znotraj ISR-ja)
static volatile int64_t zadnji_pritisk_us = 0;
#define DEBOUNCE_US 30000   // 30 ms

// ISR se sprozi ob padajocem robu (pritisk tipke, ker je aktivna nizko)
// printf ni varen v ISR-ju, zato za izpis uporabimo esp_rom_printf (IRAM-varen)
static void IRAM_ATTR tipka_isr_handler(void *arg)
{
    int64_t zdaj = esp_timer_get_time();

    // programski debounce - prehitre ponovne prekinitve (odboj kontaktov) ignoriramo
    if (zdaj - zadnji_pritisk_us < DEBOUNCE_US) {
        return;
    }
    zadnji_pritisk_us = zdaj;

    stevec_pritiskov++;
    esp_rom_printf("ISR: pritisk tipke, stevec = %lu\n", (unsigned long)stevec_pritiskov);
}

void app_main(void)
{
    // konfiguracija GPIO-jev za obe LED diodi kot izhoda
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << LED_GREEN) | (1ULL << LED_YELLOW),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);

    // konfiguracija GPIO-ja za tipko kot vhod, prekinitev ob padajocem robu (pritisk)
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_USER),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   // zunanji 10k ze je, a ne skodi
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);

    // na zacetku sta obe LED diodi ugasnjeni
    gpio_set_level(LED_GREEN,  LED_OFF);
    gpio_set_level(LED_YELLOW, LED_OFF);

    // ustvarimo naloge za utripanje obeh LED diod
    xTaskCreate(utripanje_task, "utripanje_rumena", 2048, &rumena_cfg, 5, NULL);
    xTaskCreate(utripanje_task, "utripanje_zelena", 2048, &zelena_cfg, 5, NULL);

    // namestimo servis za GPIO prekinitve in registriramo nas handler za tipko
    // stetje in izpis pritiskov se odslej v celoti izvajata znotraj ISR-ja
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_USER, tipka_isr_handler, NULL);
}
