/*
 * Predator MDK module firmware (ESP32-S3).
 *
 * Forked from the mayhem-mdk "portapack-external-module" example. Adds two
 * passive-scan features for the Predator PortaPack app:
 *   - WiFi scanning (esp_wifi, station mode, scan only)
 *   - BLE discovery (NimBLE, passive GAP scan only)
 *
 * Neither feature transmits anything beyond what a normal WiFi/BLE scan
 * requires (WiFi active-scan probe requests, which any phone sends when you
 * open its WiFi list) - there is no deauth, no beacon/SSID spam, no BLE
 * spam, and no connection attempts. The example UART passthrough app from
 * the upstream devkit is not included here since Predator's own PortaPack
 * app is a normal SD-card (.ppma) app and does not need it.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version. See the file COPYING for details.
 */

#include <iostream>

#include "driver/gpio.h"
#include "nvs_flash.h"

#include "ppi2c/pp_handler.hpp"
#include "predator_wifi.hpp"
#include "predator_ble.hpp"

#define I2C_SLAVE_SDA_IO GPIO_NUM_6
#define I2C_SLAVE_SCL_IO GPIO_NUM_5
#define ESP_SLAVE_ADDR 0x51

#define LED_RED GPIO_NUM_46
#define LED_GREEN GPIO_NUM_0
#define LED_BLUE GPIO_NUM_45

// Command IDs must match COMMAND_PREDATOR_* in
// firmware/common/i2cdev_ppmod.hpp on the PortaPack side exactly.
#define COMMAND_PREDATOR_WIFI_STARTSCAN 0xa008
#define COMMAND_PREDATOR_WIFI_STOPSCAN 0xa009
#define COMMAND_PREDATOR_WIFI_GETRESULT 0xa00a
#define COMMAND_PREDATOR_BLE_STARTSCAN 0xa00e
#define COMMAND_PREDATOR_BLE_STOPSCAN 0xa00f
#define COMMAND_PREDATOR_BLE_GETRESULT 0xa010

static void initialize_gpio() {
    gpio_install_isr_service(0);

    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);

    // Off (active-low on this board, matching the upstream devkit example).
    gpio_set_level(LED_RED, 1);
    gpio_set_level(LED_GREEN, 1);
    gpio_set_level(LED_BLUE, 1);
}

extern "C" void app_main(void) {
    initialize_gpio();

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    PPHandler::set_module_name("PREDATOR-MDK");
    PPHandler::set_module_version(1);

    PPHandler::add_custom_command(
        COMMAND_PREDATOR_WIFI_STARTSCAN, predator_wifi_on_startscan, nullptr);
    PPHandler::add_custom_command(
        COMMAND_PREDATOR_WIFI_STOPSCAN, predator_wifi_on_stopscan, nullptr);
    PPHandler::add_custom_command(
        COMMAND_PREDATOR_WIFI_GETRESULT, predator_wifi_on_getresult_cmd, predator_wifi_on_getresult_send);

    PPHandler::add_custom_command(
        COMMAND_PREDATOR_BLE_STARTSCAN, predator_ble_on_startscan, nullptr);
    PPHandler::add_custom_command(
        COMMAND_PREDATOR_BLE_STOPSCAN, predator_ble_on_stopscan, nullptr);
    PPHandler::add_custom_command(
        COMMAND_PREDATOR_BLE_GETRESULT, predator_ble_on_getresult_cmd, predator_ble_on_getresult_send);

    PPHandler::init(I2C_SLAVE_SDA_IO, I2C_SLAVE_SCL_IO, ESP_SLAVE_ADDR);

    predator_wifi_init();
    predator_ble_init();

    std::cout << "[Predator] MDK module firmware ready (WiFi scan + BLE discovery)." << std::endl;
}
