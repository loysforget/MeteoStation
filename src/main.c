#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "driver/gpio.h"
#include "dht11.h"

#define MAIN_PERIODE 3000
#define DHT11_GPIO_NUM GPIO_NUM_19

static const char TAG[] = "main";
static uint8_t temperature = 0u;
static uint8_t humidity = 0u;

void app_main(void) {
    dht11_initialisation(DHT11_GPIO_NUM);
    dht11_mesures data;
    while(1){
        data = dht11_lecture();

        if (data.status == DHT11_OK){
            temperature = data.temperature;
            humidity = data.humidity;
            ESP_LOGI(TAG, "Humidité = %d ; Température = %d\n", humidity, temperature);
            printf("TEST");
        }
        vTaskDelay(MAIN_PERIODE/portTICK_PERIOD_MS);
    }  
}