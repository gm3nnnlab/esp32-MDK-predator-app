/*
 * Predator - RX-only security research toolkit for the Mayhem MDK module.
 * See ui_predator.hpp for the scope note.
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

#include "ui_predator.hpp"

#include <cstring>

#include "portapack.hpp"
#include "receiver_model.hpp"
#include "string_format.hpp"

// Reused verbatim - no new baseband/DSP code is introduced by this app.
#include "capture_app.hpp"
#include "ui_subghzd.hpp"

using namespace portapack;

namespace ui::external_app::predator {

i2cdev::I2cDev_PPmod* get_module() {
    auto* dev = i2cdev::I2CDevManager::get_dev_by_model(I2CDECMDL_PPMOD);
    return static_cast<i2cdev::I2cDev_PPmod*>(dev);
}

// ---------------------------------------------------------------------------
// PredatorAppView
// ---------------------------------------------------------------------------
PredatorAppView::PredatorAppView(NavigationView& nav)
    : nav_{nav} {
    add_children({
        &text_title,
        &text_subtitle,
        &button_wifi,
        &button_ble,
        &button_subghz,
        &button_decode,
        &text_module_state,
    });

    button_wifi.on_select = [this](Button&) {
        nav_.push<PredatorWifiView>();
    };
    button_ble.on_select = [this](Button&) {
        nav_.push<PredatorBleView>();
    };
    button_subghz.on_select = [this](Button&) {
        nav_.push<PredatorBandPickerView>(PredatorBandTarget::Capture);
    };
    button_decode.on_select = [this](Button&) {
        nav_.push<PredatorBandPickerView>(PredatorBandTarget::Decode);
    };

    update_module_state();
}

void PredatorAppView::focus() {
    button_wifi.focus();
    update_module_state();
}

void PredatorAppView::update_module_state() {
    auto* mod = get_module();
    if (mod) {
        text_module_state.set("MDK module detected");
    } else {
        text_module_state.set("No MDK module - WiFi/BT unavailable");
    }
}

// ---------------------------------------------------------------------------
// PredatorBandPickerView
// ---------------------------------------------------------------------------
PredatorBandPickerView::PredatorBandPickerView(NavigationView& nav, PredatorBandTarget target)
    : nav_{nav}, target_{target} {
    add_children({
        &text_hint,
        &button_315,
        &button_433,
        &button_868,
        &button_915,
        &button_custom,
    });

    button_315.on_select = [this](Button&) { choose(315'000'000); };
    button_433.on_select = [this](Button&) { choose(433'920'000); };
    button_868.on_select = [this](Button&) { choose(868'350'000); };
    button_915.on_select = [this](Button&) { choose(915'000'000); };
    button_custom.on_select = [this](Button&) { choose(0); };
}

void PredatorBandPickerView::focus() {
    button_433.focus();
}

void PredatorBandPickerView::choose(uint32_t frequency_hz) {
    if (target_ == PredatorBandTarget::Capture) {
        // Stock, unmodified Capture app: raw IQ capture to SD + spectrum/RSSI.
        nav_.push<CaptureAppView>();
    } else {
        // Stock, unmodified SubGhzD app: generic OOK/FSK protocol decode.
        nav_.push<SubGhzDView>();
    }
    if (frequency_hz != 0) {
        // Retune after the target view has applied its own defaults, exactly
        // like turning the on-screen frequency knob would.
        receiver_model.set_target_frequency(frequency_hz);
    }
}

// ---------------------------------------------------------------------------
// PredatorWifiView
// ---------------------------------------------------------------------------
PredatorWifiView::PredatorWifiView(NavigationView& nav)
    : nav_{nav} {
    add_children({
        &button_scan,
        &text_status,
        &recent_entries_view,
    });

    recent_entries_view.set_parent_rect({0, header_height, screen_width, screen_height - header_height});

    button_scan.on_select = [this](Button&) { start_stop(); };

    if (!get_module()) {
        button_scan.set_focusable(false);
    } else {
        text_status.set("Ready");
    }
}

PredatorWifiView::~PredatorWifiView() {
    if (scanning_) {
        auto* mod = get_module();
        if (mod) mod->predator_wifi_stop_scan();
    }
}

void PredatorWifiView::focus() {
    button_scan.focus();
}

void PredatorWifiView::start_stop() {
    auto* mod = get_module();
    if (!mod) {
        text_status.set("No MDK module detected");
        return;
    }

    if (!scanning_) {
        recent.clear();
        recent_entries_view.set_dirty();
        next_fetch_index_ = 0;
        known_total_ = 0;

        mod->lockDevice();
        bool ok = mod->predator_wifi_start_scan();
        mod->unlockDevice();

        if (!ok) {
            text_status.set("Failed to start scan");
            return;
        }
        scanning_ = true;
        button_scan.set_text("Stop Scan");
        text_status.set("Scanning...");
    } else {
        mod->lockDevice();
        mod->predator_wifi_stop_scan();
        mod->unlockDevice();
        scanning_ = false;
        button_scan.set_text("Start Scan");
        text_status.set(known_total_ == 0 ? "No networks found" : "Scan stopped");
    }
}

void PredatorWifiView::poll_results() {
    auto* mod = get_module();
    if (!mod) return;

    mod->lockDevice();
    auto rec = mod->predator_wifi_get_result(next_fetch_index_);
    mod->unlockDevice();

    if (!rec.has_value() || !rec->valid) {
        // Nothing new yet (or the scan finished with fewer results than we
        // already have) - just wait for the next tick.
        return;
    }

    known_total_ = rec->total_count;

    uint64_t key = 0;
    memcpy(&key, rec->bssid, 6);

    auto matching = find(recent, key);
    WifiApRecentEntry entry{key};
    entry.ssid = rec->ssid[0] ? std::string(rec->ssid) : "(hidden)";
    memcpy(entry.bssid, rec->bssid, 6);
    entry.rssi = rec->rssi;
    entry.channel = rec->channel;
    entry.authmode = rec->authmode;

    if (matching != std::end(recent)) {
        *matching = entry;
    } else {
        recent.push_back(entry);
    }
    recent_entries_view.set_dirty();

    next_fetch_index_++;
    if (next_fetch_index_ >= known_total_) {
        next_fetch_index_ = 0;  // wrap around to refresh RSSI on already-seen APs
    }
}

void PredatorWifiView::on_tick() {
    if (!scanning_) return;
    // Poll roughly every ~6 frames (about 3-4 times a second) rather than
    // hammering the I2C bus on every redraw.
    if (++poll_divider_ < 6) return;
    poll_divider_ = 0;
    poll_results();
}

// ---------------------------------------------------------------------------
// PredatorBleView
// ---------------------------------------------------------------------------
PredatorBleView::PredatorBleView(NavigationView& nav)
    : nav_{nav} {
    add_children({
        &button_scan,
        &text_status,
        &recent_entries_view,
    });

    recent_entries_view.set_parent_rect({0, header_height, screen_width, screen_height - header_height});

    button_scan.on_select = [this](Button&) { start_stop(); };

    if (!get_module()) {
        button_scan.set_focusable(false);
    } else {
        text_status.set("Ready");
    }
}

PredatorBleView::~PredatorBleView() {
    if (scanning_) {
        auto* mod = get_module();
        if (mod) mod->predator_ble_stop_scan();
    }
}

void PredatorBleView::focus() {
    button_scan.focus();
}

void PredatorBleView::start_stop() {
    auto* mod = get_module();
    if (!mod) {
        text_status.set("No MDK module detected");
        return;
    }

    if (!scanning_) {
        recent.clear();
        recent_entries_view.set_dirty();
        next_fetch_index_ = 0;
        known_total_ = 0;

        mod->lockDevice();
        bool ok = mod->predator_ble_start_scan();
        mod->unlockDevice();

        if (!ok) {
            text_status.set("Failed to start scan");
            return;
        }
        scanning_ = true;
        button_scan.set_text("Stop Scan");
        text_status.set("Scanning...");
    } else {
        mod->lockDevice();
        mod->predator_ble_stop_scan();
        mod->unlockDevice();
        scanning_ = false;
        button_scan.set_text("Start Scan");
        text_status.set(known_total_ == 0 ? "No devices found" : "Scan stopped");
    }
}

void PredatorBleView::poll_results() {
    auto* mod = get_module();
    if (!mod) return;

    mod->lockDevice();
    auto rec = mod->predator_ble_get_result(next_fetch_index_);
    mod->unlockDevice();

    if (!rec.has_value() || !rec->valid) {
        return;
    }

    known_total_ = rec->total_count;

    uint64_t key = 0;
    memcpy(&key, rec->addr, 6);

    auto matching = find(recent, key);
    BleDeviceRecentEntry entry{key};
    entry.name = rec->name[0] ? std::string(rec->name) : "(no name)";
    memcpy(entry.addr, rec->addr, 6);
    entry.rssi = rec->rssi;
    entry.addr_type = rec->addr_type;

    if (matching != std::end(recent)) {
        *matching = entry;
    } else {
        recent.push_back(entry);
    }
    recent_entries_view.set_dirty();

    next_fetch_index_++;
    if (next_fetch_index_ >= known_total_) {
        next_fetch_index_ = 0;
    }
}

void PredatorBleView::on_tick() {
    if (!scanning_) return;
    if (++poll_divider_ < 6) return;
    poll_divider_ = 0;
    poll_results();
}

// ---------------------------------------------------------------------------
// Row rendering
// ---------------------------------------------------------------------------
namespace {
std::string format_mac(const uint8_t* addr) {
    std::string s{};
    s.reserve(17);
    for (int i = 0; i < 6; i++) {
        if (i) s += ":";
        s += to_string_hex(addr[i], 2);
    }
    return s;
}
}  // namespace

}  // namespace ui::external_app::predator

namespace ui {

template <>
void RecentEntriesTable<ui::external_app::predator::WifiApRecentEntries>::draw(
    const Entry& entry,
    const Rect& target_rect,
    Painter& painter,
    const Style& style,
    RecentEntriesColumns& columns) {
    std::string line = entry.ssid == "(hidden)" ? (entry.ssid + " " + external_app::predator::format_mac(entry.bssid)) : entry.ssid;
    line.resize(columns.at(0).second, ' ');
    std::string chStr = to_string_dec_uint(entry.channel);
    line += chStr + std::string(columns.at(1).second > chStr.length() ? columns.at(1).second - chStr.length() : 0, ' ');
    line += to_string_dec_int(entry.rssi);
    line.resize(target_rect.width() / 8, ' ');
    painter.draw_string(target_rect.location(), style, line);
}

template <>
void RecentEntriesTable<ui::external_app::predator::BleDeviceRecentEntries>::draw(
    const Entry& entry,
    const Rect& target_rect,
    Painter& painter,
    const Style& style,
    RecentEntriesColumns& columns) {
    std::string line = entry.name == "(no name)" ? external_app::predator::format_mac(entry.addr) : entry.name;
    line.resize(columns.at(0).second, ' ');
    std::string tyStr = to_string_dec_uint(entry.addr_type);
    line += tyStr + std::string(columns.at(1).second > tyStr.length() ? columns.at(1).second - tyStr.length() : 0, ' ');
    line += to_string_dec_int(entry.rssi);
    line.resize(target_rect.width() / 8, ' ');
    painter.draw_string(target_rect.location(), style, line);
}

}  // namespace ui
