/**
 * @file ble_force.h
 * @brief BLE Force Plate Data Streaming
 * 
 * Efficient BLE notification system for 4-channel force data:
 * - Counter (32-bit) + 4x Force values (16-bit each)
 * - Total: 12 bytes per notification
 * - Force scaled to 0.01N resolution (0-655.35N range with 16-bit)
 * - ~100 Hz notification rate for real-time streaming
 */

#ifndef BLE_FORCE_H
#define BLE_FORCE_H

#include "esp_err.h"
#include "loadcell.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BLE Force Data Packet Structure
 * Total: 10 bytes (4-channel) / 18 bytes (8-channel future)
 * 
 * timestamp_ms is elapsed time counter (milliseconds since measurement start):
 * - At 1000 Hz: increments by 1 each sample
 * - Range: 0-65,535 ms (wraps after 65.5 seconds)
 * - No date/time info, just relative timing for analysis
 * 
 * Force encoding: 0.1N resolution
 * - int16_t range: -32,768 to +32,767 → -3276.8N to +3276.7N
 * - Covers up to ±327kg force (sufficient for human GRF testing)
 */
typedef struct {
    uint16_t timestamp_ms;      // Elapsed time in ms (0-65,535, wraps at 65.5s)
    int16_t force_ch1;          // Channel 1 force in 0.1N (e.g., 1234 = 123.4N)
    int16_t force_ch2;          // Channel 2 force in 0.1N
    int16_t force_ch3;          // Channel 3 force in 0.1N
    int16_t force_ch4;          // Channel 4 force in 0.1N
} __attribute__((packed)) ble_force_packet_t;

/**
 * Initialize BLE Force Streaming Service
 * 
 * Sets up:
 * - BLE GAP (Generic Access Profile)
 * - BLE GATT server with Force Service
 * - Notification characteristic for force data
 * 
 * @param device_name BLE device name (e.g., "ZPlate")
 * @return ESP_OK on success
 */
esp_err_t ble_force_init(const char *device_name);

/**
 * Send force data notification to connected BLE client
 * 
 * Converts loadcell measurements to 16-bit scaled values and sends
 * via BLE notification if client is connected and subscribed.
 * 
 * @param loadcell Pointer to loadcell device with current measurements
 * @param timestamp_ms Elapsed time counter in ms (relative to start, not absolute time)
 * @return ESP_OK if notification sent, ESP_FAIL if not connected
 */
esp_err_t ble_force_notify(const loadcell_t *loadcell, uint16_t timestamp_ms);

/**
 * Check if BLE client is connected and subscribed to notifications
 * 
 * @return true if ready to send notifications
 */
bool ble_force_is_connected(void);

/**
 * Get number of connected BLE clients
 * 
 * @return Number of connected clients (0 or 1 for ESP32-C6)
 */
uint8_t ble_force_get_connection_count(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_FORCE_H */
