/*
 * Predator - RX-only security research toolkit for the Mayhem MDK module.
 *
 * Scope, by design: WiFi scanning, Bluetooth/BLE discovery, and SubGHz
 * signal capture / generic protocol decode (including common key-fob ISM
 * bands). This app is receive-only. It does not transmit, replay, brute
 * force, or otherwise attempt to defeat rolling-code or any other access
 * control system, and it has no RFID/NFC features.
 *
 * Only use this on systems you own or have explicit permission to test.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ui.hpp"
#include "ui_predator.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"

namespace ui::external_app::predator {
void initialize_app(ui::NavigationView& nav) {
    nav.push<PredatorAppView>();
}
}  // namespace ui::external_app::predator

extern "C" {

__attribute__((section(".external_app.app_predator.application_information"), used)) application_information_t _application_information_predator = {
    /*.memory_location = */ (uint8_t*)0x00000000,
    /*.externalAppEntry = */ ui::external_app::predator::initialize_app,
    /*.header_version = */ CURRENT_HEADER_VERSION,
    /*.app_version = */ VERSION_MD5,

    /*.app_name = */ "Predator",
    /*.bitmap_data = */
    {
        0x00,
        0x00,
        0xE0,
        0x03,
        0x18,
        0x0C,
        0x04,
        0x10,
        0x02,
        0x20,
        0x02,
        0x20,
        0xE2,
        0x23,
        0x22,
        0x22,
        0x22,
        0x22,
        0xE2,
        0x23,
        0x02,
        0x20,
        0x02,
        0x20,
        0x04,
        0x10,
        0x18,
        0x0C,
        0xE0,
        0x03,
        0x00,
        0x00},
    /*.icon_color = */ ui::Color::orange().v,
    /*.menu_location = */ app_location_t::RX,
    /*.desired_menu_position = */ -1,

    // Biggest baseband image this app touches: raw capture. The AM-audio
    // image used by the decode screen is requested dynamically at runtime.
    /*.m4_app_tag = portapack::spi_flash::image_tag_capture */ {'P', 'C', 'A', 'P'},
    /*.m4_app_offset = */ 0x00000000,  // will be filled at compile time
};
}
