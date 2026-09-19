idf_component_register(SRCS "main.cpp" "ppi2c/i2c_slave_driver.c" "ppi2c/pp_handler.cpp"
                       "predator_wifi.cpp" "predator_ble.cpp"
                       INCLUDE_DIRS "." "./ppi2c"
                       REQUIRES driver esp_driver_i2c esp_wifi esp_netif esp_event nvs_flash bt)
