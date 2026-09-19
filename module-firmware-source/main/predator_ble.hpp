/*
 * Predator MDK module firmware - BLE discovery support.
 *
 * Passive: this only ever runs a passive NimBLE GAP scan (BLE_HS_FOREVER,
 * disc_params.passive = 1). It never initiates a connection and never
 * advertises. Results are handed to the PortaPack app one record at a time
 * over the existing I2C command channel (see i2cdev_ppmod.hpp on the
 * PortaPack side for the wire format, which this must match exactly).
 *
 * Note: the ESP32-S3 radio only supports BLE, not classic Bluetooth (BR/EDR)
 * - "Bluetooth Discovery" in this app means BLE advertisement discovery.
 */
#pragma once

#include <cstdint>
#include "ppi2c/pp_structures.hpp"

// Field-for-field identical to i2cdev::I2cDev_PPmod::predator_ble_record_t
// in the mayhem-firmware repo (firmware/common/i2cdev_ppmod.hpp). Keep the
// two in sync - this is memcpy'd raw across the I2C link.
struct __attribute__((packed)) predator_ble_record_t {
    uint16_t total_count;
    uint16_t index;
    uint8_t valid;
    int8_t rssi;
    uint8_t addr_type;
    uint8_t addr[6];
    char name[32];
};

void predator_ble_init();  // call once at startup; brings up the NimBLE host

// Custom I2C command handlers (registered with PPHandler::add_custom_command).
// Matches COMMAND_PREDATOR_BLE_{STARTSCAN,STOPSCAN,GETRESULT} in
// firmware/common/i2cdev_ppmod.hpp on the PortaPack side.
void predator_ble_on_startscan(pp_command_data_t data);
void predator_ble_on_stopscan(pp_command_data_t data);
void predator_ble_on_getresult_cmd(pp_command_data_t data);   // got_command: latches the requested index
void predator_ble_on_getresult_send(pp_command_data_t data);  // send_command: returns that record
