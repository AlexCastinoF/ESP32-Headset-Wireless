#pragma once

#include "hs_link.h"

void link_master_start(void);
void link_master_set_config(const hs_hid_config_t *config);
void link_master_get_config(hs_hid_config_t *config);
void link_master_fill_status(hs_hid_status_t *status);
void link_master_send_hid(void);
