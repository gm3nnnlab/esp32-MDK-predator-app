/*
 * Predator - RX-only security research toolkit for the Mayhem MDK module.
 *
 * WiFi scan and Bluetooth/BLE discovery are bridged to the MDK external
 * module (ESP32-S3) over the existing I2cDev_PPmod link. SubGHz capture and
 * decode reuse the stock, already-shipped Capture and SubGhzD apps verbatim
 * (this app only offers quick frequency presets for common ISM / key-fob
 * bands before handing off to them) - no new baseband/DSP code is added
 * here, and nothing in this app transmits.
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

#ifndef __UI_PREDATOR_H__
#define __UI_PREDATOR_H__

#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_navigation.hpp"
#include "recent_entries.hpp"
#include "i2cdev_ppmod.hpp"

#include <string>

namespace ui::external_app::predator {

// ---------------------------------------------------------------------------
// Shared helper: look up the MDK module driver, if one is plugged in.
// ---------------------------------------------------------------------------
i2cdev::I2cDev_PPmod* get_module();

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------
class PredatorAppView : public View {
   public:
    PredatorAppView(NavigationView& nav);

    void focus() override;
    std::string title() const override { return "Predator"; };

   private:
    NavigationView& nav_;

    Text text_title{
        {0, 0, screen_width, 16},
        "Predator - RX security toolkit"};

    Text text_subtitle{
        {0, 16, screen_width, 16},
        "Scan / capture only - your own gear"};

    Button button_wifi{
        {0, 40, screen_width, 32},
        "WiFi Scan"};

    Button button_ble{
        {0, 76, screen_width, 32},
        "Bluetooth Discovery"};

    Button button_subghz{
        {0, 112, screen_width, 32},
        "SubGHz Capture (incl. key fobs)"};

    Button button_decode{
        {0, 148, screen_width, 32},
        "Signal Decode (incl. key fobs)"};

    Text text_module_state{
        {0, 190, screen_width, 16},
        ""};

    void update_module_state();
};

// ---------------------------------------------------------------------------
// Frequency preset helper used by both the SubGHz capture and decode screens.
// Presets cover the common license-free ISM bands most key fobs, garage/gate
// remotes, weather sensors, and similar SubGHz devices use.
// ---------------------------------------------------------------------------
enum class PredatorBandTarget {
    Capture,  // hands off to the stock CaptureAppView
    Decode    // hands off to the stock SubGhzDView
};

class PredatorBandPickerView : public View {
   public:
    PredatorBandPickerView(NavigationView& nav, PredatorBandTarget target);

    void focus() override;
    std::string title() const override {
        return target_ == PredatorBandTarget::Capture ? "SubGHz Capture" : "Signal Decode";
    };

   private:
    NavigationView& nav_;
    PredatorBandTarget target_;

    void choose(uint32_t frequency_hz);

    Text text_hint{
        {0, 0, screen_width, 16},
        "Pick a band, then capture/decode"};

    Button button_315{{0, 24, screen_width, 28}, "315.000 MHz (fob/remote)"};
    Button button_433{{0, 56, screen_width, 28}, "433.920 MHz (fob/remote)"};
    Button button_868{{0, 88, screen_width, 28}, "868.350 MHz (fob/remote)"};
    Button button_915{{0, 120, screen_width, 28}, "915.000 MHz (fob/remote)"};
    Button button_custom{{0, 152, screen_width, 28}, "Custom / tune manually"};
};

// ---------------------------------------------------------------------------
// WiFi scan
// ---------------------------------------------------------------------------
struct WifiApRecentEntry {
    using Key = uint64_t;
    static constexpr Key invalid_key = 0xFFFFFFFFFFFFFFFFull;

    uint64_t bssid_key{invalid_key};
    std::string ssid{};
    uint8_t bssid[6]{0, 0, 0, 0, 0, 0};
    int8_t rssi{0};
    uint8_t channel{0};
    uint8_t authmode{0};

    WifiApRecentEntry() {}
    WifiApRecentEntry(Key key) : bssid_key{key} {}

    Key key() const { return bssid_key; }
};
using WifiApRecentEntries = RecentEntries<WifiApRecentEntry>;
using WifiApRecentEntriesView = RecentEntriesView<WifiApRecentEntries>;

class PredatorWifiView : public View {
   public:
    PredatorWifiView(NavigationView& nav);
    ~PredatorWifiView();

    void focus() override;
    std::string title() const override { return "WiFi Scan"; };

   private:
    NavigationView& nav_;

    bool scanning_{false};
    uint8_t poll_divider_{0};
    uint16_t next_fetch_index_{0};
    uint16_t known_total_{0};

    WifiApRecentEntries recent{};

    void start_stop();
    void poll_results();
    void on_tick();

    Button button_scan{
        {0, 0, screen_width, 24},
        "Start Scan"};

    Text text_status{
        {0, 24, screen_width, 16},
        "Module not detected"};

    static constexpr auto header_height = 40;

    RecentEntriesColumns columns{{
        {"SSID / BSSID", 22},
        {"Ch", 3},
        {"RSSI", 5},
    }};
    WifiApRecentEntriesView recent_entries_view{columns, recent};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) { this->on_tick(); }};
};

// ---------------------------------------------------------------------------
// Bluetooth / BLE discovery
// ---------------------------------------------------------------------------
struct BleDeviceRecentEntry {
    using Key = uint64_t;
    static constexpr Key invalid_key = 0xFFFFFFFFFFFFFFFFull;

    uint64_t addr_key{invalid_key};
    std::string name{};
    uint8_t addr[6]{0, 0, 0, 0, 0, 0};
    int8_t rssi{0};
    uint8_t addr_type{0};

    BleDeviceRecentEntry() {}
    BleDeviceRecentEntry(Key key) : addr_key{key} {}

    Key key() const { return addr_key; }
};
using BleDeviceRecentEntries = RecentEntries<BleDeviceRecentEntry>;
using BleDeviceRecentEntriesView = RecentEntriesView<BleDeviceRecentEntries>;

class PredatorBleView : public View {
   public:
    PredatorBleView(NavigationView& nav);
    ~PredatorBleView();

    void focus() override;
    std::string title() const override { return "Bluetooth Discovery"; };

   private:
    NavigationView& nav_;

    bool scanning_{false};
    uint8_t poll_divider_{0};
    uint16_t next_fetch_index_{0};
    uint16_t known_total_{0};

    BleDeviceRecentEntries recent{};

    void start_stop();
    void poll_results();
    void on_tick();

    Button button_scan{
        {0, 0, screen_width, 24},
        "Start Scan"};

    Text text_status{
        {0, 24, screen_width, 16},
        "Module not detected"};

    static constexpr auto header_height = 40;

    RecentEntriesColumns columns{{
        {"Address / Name", 22},
        {"Typ", 3},
        {"RSSI", 5},
    }};
    BleDeviceRecentEntriesView recent_entries_view{columns, recent};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) { this->on_tick(); }};
};

}  // namespace ui::external_app::predator

#endif /*__UI_PREDATOR_H__*/
