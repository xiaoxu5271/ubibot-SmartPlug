/* Copyright (c) 2017 pcbreflux. All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. *
 */
#ifndef __BLUFI_GATTS_PROFILE_H__
#define __BLUFI_GATTS_PROFILE_H__

#include "esp_bt.h"
//#include "bta_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#define BLEOK    0x01
#define BLEERR   0x00

char ble_dev_pwd[16];

void ble_app_start(void);
void gap_init(void);
void gap_start(void);
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

typedef enum
{
    BLU_JSON_FORMAT_ERROR,
    BLU_RESULT_SUCCESS,
    BLU_PWD_REFUSE,
    BLU_WRITE_FLASH_OVER,
    BLU_NO_WIFI_SSID,
    BLU_NO_WIFI_PWD,
    BLU_WIFI_ERR
} BLU_error_info;

typedef struct blufi_beacon_s blufi_beacon_t;

char SerialNum[17];
char ProductId[33];
char BleName[20];
char ApiKey[33];
char ChannelId[17];

uint8_t Ble_mes_status;

//#define PROFILE_NUM 1
//#define PROFILE_APP_ID 0

#endif /* __BLUFI_GATTS_PROFILE_H__ */