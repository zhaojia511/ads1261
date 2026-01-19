/**
 * @file ble_force.c
 * @brief BLE Force Plate Data Streaming Implementation
 */

#include <string.h>
#include "ble_force.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BLE_FORCE";

/* BLE Service and Characteristic UUIDs */
#define FORCE_SERVICE_UUID          0x1815  // Generic service for force data
#define FORCE_CHAR_UUID             0x2A58  // Custom characteristic for notifications

/* BLE Configuration */
#define FORCE_PROFILE_NUM           1
#define FORCE_PROFILE_APP_IDX       0
#define FORCE_APP_ID                0x55

/* Connection State */
static bool ble_connected = false;
static bool notification_enabled = false;
static uint16_t conn_id = 0;
static esp_gatt_if_t gatts_if_global = ESP_GATT_IF_NONE;
static uint16_t force_handle = 0;

/* BLE Advertising Data */
static uint8_t adv_config_done = 0;
#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,  // 20ms
    .adv_int_max        = 0x40,  // 40ms
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* Service and Characteristic Attributes */
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t force_service_uuid[2] = {FORCE_SERVICE_UUID & 0xFF, (FORCE_SERVICE_UUID >> 8) & 0xFF};
static uint8_t force_char_uuid[2] = {FORCE_CHAR_UUID & 0xFF, (FORCE_CHAR_UUID >> 8) & 0xFF};

/* Client Configuration Descriptor (enable/disable notifications) */
static uint8_t force_ccc[2] = {0x00, 0x00};

/* Attribute database */
enum {
    IDX_SVC,
    IDX_CHAR_DECL,
    IDX_CHAR_VAL,
    IDX_CHAR_CFG,
    
    HRS_IDX_NB,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static struct gatts_profile_inst force_profile_tab[FORCE_PROFILE_NUM] = {
    [FORCE_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed");
        } else {
            ESP_LOGI(TAG, "Advertising started");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Connection params updated: status=%d, min_int=%d, max_int=%d, latency=%d, timeout=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        
        esp_ble_gap_set_device_name("ZPlate");
        
        esp_ble_gap_config_adv_data_raw((uint8_t[]){
            0x02, 0x01, 0x06,  // Flags: General Discoverable
            0x03, 0x03, FORCE_SERVICE_UUID & 0xFF, (FORCE_SERVICE_UUID >> 8) & 0xFF,  // Complete list of 16-bit UUIDs
        }, 8);
        
        adv_config_done |= ADV_CONFIG_FLAG;
        
        esp_ble_gatts_create_attr_tab((esp_gatts_attr_db_t[]){
            // Service Declaration
            [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, 2, 2, force_service_uuid}},
            
            // Characteristic Declaration
            [IDX_CHAR_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, 1, 1, (uint8_t *)&char_prop_read_notify}},
            
            // Characteristic Value
            [IDX_CHAR_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, force_char_uuid, ESP_GATT_PERM_READ, sizeof(ble_force_packet_t), 0, NULL}},
            
            // Client Characteristic Configuration Descriptor
            [IDX_CHAR_CFG] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 0, NULL}},
        }, HRS_IDX_NB, gatts_if, param->reg.app_id);
        
        break;
    }
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "GATT_READ_EVT, conn_id %d, trans_id %lu, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);
        break;
    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep) {
            ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            
            // Check if this is CCC write (enable/disable notifications)
            if (param->write.len == 2) {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001) {
                    ESP_LOGI(TAG, "Notifications enabled by client");
                    notification_enabled = true;
                } else if (descr_value == 0x0000) {
                    ESP_LOGI(TAG, "Notifications disabled by client");
                    notification_enabled = false;
                }
            }
        }
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_CONF_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_CONNECT_EVT: {
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
        esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);
        
        conn_id = param->connect.conn_id;
        gatts_if_global = gatts_if;
        ble_connected = true;
        
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x10;    // 20ms
        conn_params.min_int = 0x10;    // 20ms
        conn_params.timeout = 400;     // 4s
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
        ble_connected = false;
        notification_enabled = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        } else if (param->add_attr_tab.num_handle != HRS_IDX_NB) {
            ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)", 
                     param->add_attr_tab.num_handle, HRS_IDX_NB);
        } else {
            ESP_LOGI(TAG, "Create attribute table successfully, the number handle = %d", param->add_attr_tab.num_handle);
            force_handle = param->add_attr_tab.handles[IDX_CHAR_VAL];
            esp_ble_gatts_start_service(param->add_attr_tab.handles[IDX_SVC]);
        }
        break;
    }
    case ESP_GATTS_STOP_EVT:
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    case ESP_GATTS_UNREG_EVT:
    case ESP_GATTS_DELETE_EVT:
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            force_profile_tab[FORCE_PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }
    
    for (int idx = 0; idx < FORCE_PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || gatts_if == force_profile_tab[idx].gatts_if) {
            if (force_profile_tab[idx].gatts_cb) {
                force_profile_tab[idx].gatts_cb(event, gatts_if, param);
            }
        }
    }
}

esp_err_t ble_force_init(const char *device_name)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing BLE Force Streaming...");
    
    // Release BLE/Classic Bluetooth memory (we only need BLE)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Initialize controller failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Enable controller failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Init bluedroid failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Enable bluedroid failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register callbacks
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_ble_gatts_app_register(FORCE_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set MTU to 512 bytes for efficient data transfer
    ret = esp_ble_gatt_set_local_mtu(512);
    if (ret) {
        ESP_LOGE(TAG, "Set local MTU failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "BLE Force Streaming initialized successfully");
    ESP_LOGI(TAG, "Device name: %s", device_name ? device_name : "ZPlate");
    
    return ESP_OK;
}

esp_err_t ble_force_notify(const loadcell_t *loadcell, uint16_t timestamp_ms)
{
    if (!ble_connected || !notification_enabled || gatts_if_global == ESP_GATT_IF_NONE) {
        return ESP_FAIL;
    }
    
    ble_force_packet_t packet;
    packet.timestamp_ms = timestamp_ms;
    
    // Convert force (Newtons) to 16-bit scaled values (0.1N resolution)
    // Example: 123.4N -> 1234
    // Range: -3276.8N to +3276.7N with 0.1N resolution
    for (int i = 0; i < 4; i++) {
        float force_n = loadcell->measurements[i].force_newtons;
        int32_t scaled = (int32_t)(force_n * 10.0f);  // Scale to 0.1N
        
        // Clamp to 16-bit range
        if (scaled > 32767) scaled = 32767;
        if (scaled < -32768) scaled = -32768;
        
        switch (i) {
            case 0: packet.force_ch1 = (int16_t)scaled; break;
            case 1: packet.force_ch2 = (int16_t)scaled; break;
            case 2: packet.force_ch3 = (int16_t)scaled; break;
            case 3: packet.force_ch4 = (int16_t)scaled; break;
        }
    }
    
    // Send notification
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if_global, conn_id, force_handle,
                                                sizeof(packet), (uint8_t *)&packet, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Send notification failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

bool ble_force_is_connected(void)
{
    return ble_connected && notification_enabled;
}

uint8_t ble_force_get_connection_count(void)
{
    return ble_connected ? 1 : 0;
}
