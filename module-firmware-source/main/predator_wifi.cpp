#include "predator_wifi.hpp"

#include <cstring>
#include <vector>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "predator_wifi";

static constexpr uint16_t MAX_RESULTS = 64;

static volatile bool s_scan_active = false;
static volatile uint16_t s_pending_index = 0;

// Only ever touched by predator_wifi_task (written) and the I2C send
// callback (read-only, by index). Not mutex-protected - same informal
// safety level the shipped uart example in this project already uses for
// its own shared queue.
static std::vector<predator_wifi_record_t> s_results;

static void predator_wifi_task(void* arg) {
    (void)arg;
    wifi_ap_record_t raw[MAX_RESULTS];

    while (true) {
        if (!s_scan_active) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        wifi_scan_config_t scan_cfg = {};
        scan_cfg.show_hidden = true;
        // Blocking scan across all channels. This runs in our own task, not
        // an ISR, so blocking here is fine.
        esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "scan_start failed: %d", (int)err);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        uint16_t num = MAX_RESULTS;
        if (esp_wifi_scan_get_ap_records(&num, raw) != ESP_OK) {
            num = 0;
        }
        if (num > MAX_RESULTS) num = MAX_RESULTS;

        std::vector<predator_wifi_record_t> built;
        built.reserve(num);
        for (uint16_t i = 0; i < num; i++) {
            predator_wifi_record_t r{};
            r.total_count = num;
            r.index = i;
            r.valid = 1;
            r.rssi = (int8_t)raw[i].rssi;
            r.channel = raw[i].primary;
            r.authmode = (uint8_t)raw[i].authmode;
            memcpy(r.bssid, raw[i].bssid, 6);
            memset(r.ssid, 0, sizeof(r.ssid));
            memcpy(r.ssid, raw[i].ssid, sizeof(r.ssid) - 1 < sizeof(raw[i].ssid) ? sizeof(r.ssid) - 1 : sizeof(raw[i].ssid));
            built.push_back(r);
        }

        // Single assignment: minimizes the window where a concurrent
        // GETRESULT read could see a half-updated vector.
        s_results = std::move(built);
    }
}

void predator_wifi_init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    xTaskCreate(predator_wifi_task, "predator_wifi", 4096, nullptr, 5, nullptr);
}

void predator_wifi_on_startscan(pp_command_data_t /*data*/) {
    s_scan_active = true;
}

void predator_wifi_on_stopscan(pp_command_data_t /*data*/) {
    s_scan_active = false;
}

void predator_wifi_on_getresult_cmd(pp_command_data_t data) {
    if (data.data && data.data->size() == 2) {
        s_pending_index = *(uint16_t*)data.data->data();
    }
}

void predator_wifi_on_getresult_send(pp_command_data_t data) {
    predator_wifi_record_t r{};
    uint16_t idx = s_pending_index;
    if (idx < s_results.size()) {
        r = s_results[idx];
    } else {
        r.total_count = (uint16_t)s_results.size();
        r.index = idx;
        r.valid = 0;
    }
    data.data->resize(sizeof(r));
    memcpy(data.data->data(), &r, sizeof(r));
}
