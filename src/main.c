/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

void wifi_unit_demo();
void wifi_connect();
void node_entry();
void gpio_init();
void gpio_control_test();
void ble_entry();

#include "pch.h"

void app_main(void)
{
    printf("Hello world!\n");
    // wifi_unit_demo();
    wifi_connect();
    gpio_init();
    node_entry();
    // gpio_control_test();
    // ble_entry();
}