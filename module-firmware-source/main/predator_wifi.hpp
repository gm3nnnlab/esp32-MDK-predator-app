/*
 * Predator MDK module firmware - WiFi scan support.
 *
 * Passive: this only ever calls esp_wifi_scan_start(), it never associates,
 * transmits probe floods, deauth frames, or anything else beyond a normal
 * scan. Results are handed to the PortaPack app one record at a time over
 * the existing I2C command channel (see i2cdev_ppmod.hpp on the PortaPack
 * side for the wire format, which this must match exactly).
 */
#pragma once

#include <cstdint>
#include "ppi2c/pp_structures.hpp"

// Field-for-field identical to i2cdev::I2cDev_PPmod::predator_wifi_record_t
// in the mayhem-firmware repo (firmware/common/i2cdev_ppmod.hpp). Keep the
// two in sync - this is memcpy'd raw across the I2C link.
struct __attribute__((packed)) predator_wifi_record_t {
    uint16_t total_count;
    uint16_t index;
    uint8_t valid;
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode;
    uint8_t bssid[6];
    char ssid[33];
};

void predator_wifi_init();  // call once at startup; sets up the WiFi driver in STA mode

// Custom I2C command handlers (registered with PPHandler::add_custom_command).
// Matches COMMAND_PREDATOR_WIFI_{STARTSCAN,STOPSCAN,GETRESULT} in
// firmware/common/i2cdev_ppmod.hpp on the PortaPack side.
void predator_wifi_on_startscan(pp_command_data_t data);
void predator_wifi_on_stopscan(pp_command_data_t data);
void predator_wifi_on_getresult_cmd(pp_command_data_t data);   // got_command: latches the requested index
void predator_wifi_on_getresult_send(pp_command_data_t data);  // send_command: returns that record
