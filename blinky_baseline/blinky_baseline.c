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

void app_main(void)
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << LED_GREEN) | (1ULL << LED_YELLOW),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_USER),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   // zunanji 10k ze je, a ne skodi
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    gpio_set_level(LED_GREEN,  LED_OFF);
    gpio_set_level(LED_YELLOW, LED_OFF);

    int stanje = 0;

    while (1) {
        stanje = !stanje;
        gpio_set_level(LED_YELLOW, stanje ? LED_ON : LED_OFF);

        // tipka je aktivna v nizkem stanju
        bool pritisnjena = (gpio_get_level(BTN_USER) == 0);
        gpio_set_level(LED_GREEN, pritisnjena ? LED_ON : LED_OFF);

        printf("utrip: %d, tipka: %s\n", stanje, pritisnjena ? "DA" : "NE");

        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
