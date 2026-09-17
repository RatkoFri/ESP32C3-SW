#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

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

// Naloga, ki samo periodicno bere stanje tipke in ga izpise
static void tipka_task(void *arg)
{
    while (1) {
        // tipka je aktivna v nizkem stanju
        bool pritisnjena = (gpio_get_level(BTN_USER) == 0);
        printf("tipka: %s\n", pritisnjena ? "DA" : "NE");

        vTaskDelay(pdMS_TO_TICKS(150));
    }
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

    // konfiguracija GPIO-ja za tipko kot vhod
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_USER),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   // zunanji 10k ze je, a ne skodi
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    // na zacetku sta obe LED diodi ugasnjeni
    gpio_set_level(LED_GREEN,  LED_OFF);
    gpio_set_level(LED_YELLOW, LED_OFF);

    // ustvarimo tri neodvisne FreeRTOS naloge:
    // - utripanje rumene LED
    // - utripanje zelene LED
    // - branje tipke
    xTaskCreate(utripanje_task, "utripanje_rumena", 2048, &rumena_cfg, 5, NULL);
    xTaskCreate(utripanje_task, "utripanje_zelena", 2048, &zelena_cfg, 5, NULL);
    xTaskCreate(tipka_task,     "tipka",            2048, NULL,        5, NULL);
}
