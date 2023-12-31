/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "util.h"

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output (ESP32C2/ESP32H2 uses GPIO8 as the second output pin)
 * GPIO19: output (ESP32C2/ESP32H2 uses GPIO9 as the second output pin)
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Note. These are the default GPIO pins to be used in the example. You can
 * change IO pins in menuconfig.
 *
 * Test:
 * Connect GPIO18(8) with GPIO4
 * Connect GPIO19(9) with GPIO5
 * Generate pulses on GPIO18(8)/19(9), that triggers interrupt on GPIO4/5
 *
 */

// #define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
// #define GPIO_OUTPUT_IO_1    CONFIG_GPIO_OUTPUT_1
// #define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
// #define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
// #define GPIO_INPUT_IO_1     CONFIG_GPIO_INPUT_1
// #define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
// #define ESP_INTR_FLAG_DEFAULT 0

#define GPIO_OUTPUT_IO_2 2
#define GPIO_OUTPUT_IO_18 18
#define GPIO_OUTPUT_IO_19 19
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO_18 | 1ULL << GPIO_OUTPUT_IO_19 | 1ULL << GPIO_OUTPUT_IO_2)

static QueueHandle_t gpio_evt_queue = NULL;
// zero-initialize the config structure.
static gpio_config_t io_conf = {};

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%" PRIu32 "] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void store_gpio_val(uint32_t *data)
{
    uint64_t gpio_mask = io_conf.pin_bit_mask;
    int size = sizeof(uint64_t) * 8;

    uint64_t mask = 1;
    for (int i = 0; i < size; i++)
    {
        if ((mask << i & gpio_mask))
        {
            int val = gpio_get_level(i);
            set_bit_value(data, i, val);
        }
    }
}

uint64_t get_gpio_mask()
{
    return io_conf.pin_bit_mask;
}

void gpio_control(int pin, int lv)
{
    // TODO: check pin value
    gpio_set_level(pin, lv);
}

void gpio_control_test()
{
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);

    int cnt = 0;
    while (1)
    {
        printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_OUTPUT_IO_18, cnt % 2);
        gpio_set_level(GPIO_OUTPUT_IO_19, cnt % 2);
        gpio_set_level(GPIO_OUTPUT_IO_2, cnt % 2);
    }
}

void gpio_init()
{
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);
}