#include "tusb.h"
#include "usb_descriptors.h"

#include "hs_link.h"

#include <string.h>

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = HS_USB_VID,
    .idProduct = HS_USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + HS_AUDIO_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_hid_report[] = {
    0x06, 0x00, 0xFF,
    0x09, 0x01,
    0xA1, 0x01,
    0x85, HS_HID_REPORT_STATUS,
    0x09, 0x01,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, sizeof(hs_hid_status_t),
    0x81, 0x02,
    0x85, HS_HID_REPORT_CONFIG,
    0x09, 0x02,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, sizeof(hs_hid_config_t),
    0xB1, 0x02,
    0xC0,
};

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 500),
    HS_AUDIO_DESCRIPTOR(EPNUM_AUDIO_OUT, EPNUM_AUDIO_IN | 0x80),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 5, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
                       EPNUM_HID_IN | 0x80, CFG_TUD_HID_EP_BUFSIZE, 10),
};

TU_VERIFY_STATIC(sizeof(desc_configuration) == CONFIG_TOTAL_LEN, "tamanho do descritor USB");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Headset Wireless",
    "Headset Wireless",
    "HS-DONGLE",
    "Headset Wireless",
    "Controle do headset",
};

static uint16_t desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count = 0;
    if (index == 0) {
        memcpy(&desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
        return NULL;
    } else {
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 32) {
            chr_count = 32;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str[1 + i] = (uint16_t)str[i];
        }
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}
