#include "predator_ble.hpp"

#include <cstring>
#include <vector>
#include <algorithm>

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

static const char* TAG = "predator_ble";

static constexpr size_t MAX_RESULTS = 64;

static volatile uint16_t s_pending_index = 0;
static uint8_t s_own_addr_type = 0;

// Written from the NimBLE host task (GAP event callback), read by index
// from the I2C send callback. Same informal safety level as predator_wifi.cpp
// and the project's own shipped uart example.
static std::vector<predator_ble_record_t> s_results;

static predator_ble_record_t* find_or_add(const uint8_t addr[6], uint8_t addr_type) {
    for (auto& r : s_results) {
        if (r.addr_type == addr_type && memcmp(r.addr, addr, 6) == 0) {
            return &r;
        }
    }
    if (s_results.size() >= MAX_RESULTS) {
        return nullptr;  // table full - keep whatever we already have
    }
    predator_ble_record_t r{};
    memcpy(r.addr, addr, 6);
    r.addr_type = addr_type;
    r.valid = 1;
    s_results.push_back(r);
    return &s_results.back();
}

static void start_scan();

static int ble_gap_event_cb(struct ble_gap_event* event, void* arg) {
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        const auto& d = event->disc;
        auto* rec = find_or_add(d.addr.val, d.addr.type);
        if (rec) {
            rec->rssi = (int8_t)d.rssi;

            struct ble_hs_adv_fields fields;
            if (ble_hs_adv_parse_fields(&fields, d.data, d.length_data) == 0 && fields.name != nullptr && fields.name_len > 0) {
                size_t len = fields.name_len < sizeof(rec->name) - 1 ? fields.name_len : sizeof(rec->name) - 1;
                memcpy(rec->name, fields.name, len);
                rec->name[len] = 0;
            }

            for (auto& r : s_results) r.total_count = (uint16_t)s_results.size();
        }
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        // BLE_HS_FOREVER scans don't normally complete on their own, but if
        // the stack ever stops us, just try again.
        start_scan();
    }
    return 0;
}

static void start_scan() {
    if (ble_gap_disc_active()) return;

    struct ble_gap_disc_params params{};
    params.passive = 1;          // never send scan requests - listen only
    params.filter_duplicates = 0;  // keep refreshing RSSI on already-seen devices

    int rc = ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &params, ble_gap_event_cb, nullptr);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_disc failed: %d", rc);
    }
}

static void on_sync(void) {
    ble_hs_util_ensure_addr(0);
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGW(TAG, "id_infer_auto failed: %d", rc);
        s_own_addr_type = BLE_OWN_ADDR_PUBLIC;
    }
    // Do NOT auto-start: wait for the PortaPack app to request a scan via
    // COMMAND_PREDATOR_BLE_STARTSCAN.
}

static void on_reset(int reason) {
    ESP_LOGW(TAG, "nimble host reset, reason %d", reason);
}

static void ble_host_task(void* param) {
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void predator_ble_init() {
    nimble_port_init();

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;

    nimble_port_freertos_init(ble_host_task);
}

void predator_ble_on_startscan(pp_command_data_t /*data*/) {
    start_scan();
}

void predator_ble_on_stopscan(pp_command_data_t /*data*/) {
    if (ble_gap_disc_active()) {
        ble_gap_disc_cancel();
    }
}

void predator_ble_on_getresult_cmd(pp_command_data_t data) {
    if (data.data && data.data->size() == 2) {
        s_pending_index = *(uint16_t*)data.data->data();
    }
}

void predator_ble_on_getresult_send(pp_command_data_t data) {
    predator_ble_record_t r{};
    uint16_t idx = s_pending_index;
    if (idx < s_results.size()) {
        r = s_results[idx];
        r.total_count = (uint16_t)s_results.size();
    } else {
        r.total_count = (uint16_t)s_results.size();
        r.index = idx;
        r.valid = 0;
    }
    r.index = idx;
    data.data->resize(sizeof(r));
    memcpy(data.data->data(), &r, sizeof(r));
}
