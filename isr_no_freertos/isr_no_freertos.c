#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"
#include "esp_attr.h"   // IRAM_ATTR - prej smo ga dobili posredno preko freertos/portmacro.h

// Opomba: app_main v ESP-IDF sicer vedno formalno tece kot ena FreeRTOS naloga
// (tega spodaj lezeci mehanizem ne omogoca zares odstraniti), a ta datoteka
// ne uporablja NOBENE FreeRTOS funkcije - ni xTaskCreate, ni vTaskDelay, ni
// TaskHandle_t. Casovno usklajevanje temelji izkljucno na esp_timer_get_time(),
// pavza med iteracijami pa je busy-wait preko esp_rom_delay_us (aktivno
// cakanje, ne sprostitev CPE-ja schedulerju). Ker CPE zato nikoli ne preide
// na idle nalogo, na zacetku izklopimo task watchdog (esp_task_wdt_deinit),
// sicer bi ta po nekaj sekundah ponastavil napravo.

#define LED_GREEN   GPIO_NUM_6
#define LED_YELLOW  GPIO_NUM_7
#define BTN_USER    GPIO_NUM_10

// LED-ici imata katodo na GPIO -> 0 = prizgano, 1 = ugasnjeno
#define LED_ON  0
#define LED_OFF 1

#define PERIODA_RUMENA_US  (150 * 1000)
#define PERIODA_ZELENA_US  (300 * 1000)

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
    // izklopimo task watchdog - spodnji loop nikoli ne sprosti CPE-ja
    // schedulerju (ni vTaskDelay/yield), zato bi ga sicer watchdog ponastavil
    esp_task_wdt_deinit();

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

    // namestimo servis za GPIO prekinitve in registriramo nas handler za tipko
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_USER, tipka_isr_handler, NULL);

    int stanje_rumena = 0;
    int stanje_zelena = 0;
    int64_t naslednji_rumena_us = esp_timer_get_time();
    int64_t naslednji_zelena_us = esp_timer_get_time();

    // en sam zaporeden loop, brez ikakrsne FreeRTOS funkcije za utripanje
    while (1) {
        int64_t zdaj = esp_timer_get_time();

        if (zdaj >= naslednji_rumena_us) {
            stanje_rumena = !stanje_rumena;
            gpio_set_level(LED_YELLOW, stanje_rumena ? LED_ON : LED_OFF);
            printf("utrip rumena: %d\n", stanje_rumena);
            naslednji_rumena_us += PERIODA_RUMENA_US;
        }

        if (zdaj >= naslednji_zelena_us) {
            stanje_zelena = !stanje_zelena;
            gpio_set_level(LED_GREEN, stanje_zelena ? LED_ON : LED_OFF);
            printf("utrip zelena: %d\n", stanje_zelena);
            naslednji_zelena_us += PERIODA_ZELENA_US;
        }

        // busy-wait namesto vTaskDelay - aktivno cakanje, brez klica v FreeRTOS
        esp_rom_delay_us(1000);
    }
}
