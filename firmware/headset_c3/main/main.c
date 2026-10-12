#include "link_slave.h"

#include "esp_log.h"

void app_main(void) {
    ESP_LOGI("hs_c3", "Headset Wireless fone 1.0.0");
    link_slave_start();
}
