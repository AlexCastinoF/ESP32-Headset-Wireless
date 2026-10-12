#include "link_master.h"
#include "usb_audio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    ESP_LOGI("hs_s3", "Headset Wireless dongle %s", "1.0.0");
    link_master_start();
    usb_audio_start();
}
