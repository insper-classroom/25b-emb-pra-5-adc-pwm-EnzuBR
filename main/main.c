#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pins.h"

typedef struct {
    uint8_t axis;
    int16_t val;
} adc_t;

QueueHandle_t xQueueADC;

#define ADC_WINDOW_SIZE 10
#define ADC_CENTER 2048
#define ADC_DEAD_ZONE 150
#define MOUSE_MAX_SPEED 10

void adc_task(void *p) {
    uint8_t adc_channel = (uint8_t)(uintptr_t)p;
    uint8_t axis = adc_channel;
    uint16_t samples[ADC_WINDOW_SIZE] = {0};
    uint32_t sum = 0;
    int index = 0;

    while (1) {
        adc_select_input(adc_channel);
        uint16_t raw_val = adc_read();

        sum -= samples[index];
        samples[index] = raw_val;
        sum += raw_val;
        index = (index + 1) % ADC_WINDOW_SIZE;
        uint16_t filtered_val = sum / ADC_WINDOW_SIZE;

        int16_t mouse_val = 0;
        if (filtered_val > ADC_CENTER + ADC_DEAD_ZONE) {
            mouse_val = (filtered_val - (ADC_CENTER + ADC_DEAD_ZONE)) * MOUSE_MAX_SPEED / (4095 - (ADC_CENTER + ADC_DEAD_ZONE));
        } else if (filtered_val < ADC_CENTER - ADC_DEAD_ZONE) {
            mouse_val = (filtered_val - (ADC_CENTER - ADC_DEAD_ZONE)) * MOUSE_MAX_SPEED / (ADC_CENTER - ADC_DEAD_ZONE);
        }
        
        if (mouse_val > MOUSE_MAX_SPEED) mouse_val = MOUSE_MAX_SPEED;
        if (mouse_val < -MOUSE_MAX_SPEED) mouse_val = -MOUSE_MAX_SPEED;

        if (mouse_val != 0) {
            adc_t data = {
                .axis = axis,
                .val = mouse_val
            };
            xQueueSend(xQueueADC, &data, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void uart_task(void *p) {
    adc_t data;
    while (1) {
        if (xQueueReceive(xQueueADC, &data, portMAX_DELAY) == pdPASS) {
            uint8_t lsb = data.val & 0xFF;
            uint8_t msb = (data.val >> 8) & 0xFF;

            putchar_raw(0xFF);
            putchar_raw(data.axis);
            putchar_raw(lsb);
            putchar_raw(msb);
        }
    }
}

int main() {
    stdio_init_all();
    
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    xQueueADC = xQueueCreate(16, sizeof(adc_t));

    xTaskCreate(adc_task, "X_Task", 256, (void *)0, 1, NULL);
    xTaskCreate(adc_task, "Y_Task", 256, (void *)1, 1, NULL);
    xTaskCreate(uart_task, "UART_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}