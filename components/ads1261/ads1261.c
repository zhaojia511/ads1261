#include "ads1261.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ADS1261";

/* ADS1261 Commands */
#define ADS1261_CMD_RESET       0x06
#define ADS1261_CMD_START       0x08
#define ADS1261_CMD_STOP        0x0A
#define ADS1261_CMD_RDATA       0x12
#define ADS1261_CMD_RREG        0x20
#define ADS1261_CMD_WREG        0x40

#define ADS1261_TIMEOUT         1000

esp_err_t ads1261_init(ads1261_t *device, spi_bus_config_t *spi_bus_cfg, int cs_pin, int drdy_pin)
{
    if (!device) {
        return ESP_ERR_INVALID_ARG;
    }

    device->cs_pin = cs_pin;
    device->drdy_pin = drdy_pin;

    /* Initialize GPIO for CS and DRDY */
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << cs_pin) | (1ULL << drdy_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_cfg);
    gpio_set_level(cs_pin, 1);

    /* Initialize SPI bus */
    spi_device_interface_config_t spi_cfg = {
        .mode = 1,
        .clock_speed_hz = 1 * 1000 * 1000,  // 1 MHz
        .spics_io_num = -1,  // Use GPIO for CS
        .queue_size = 7,
    };

    esp_err_t ret = spi_bus_add_device(HSPI_HOST, &spi_cfg, &device->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return ret;
    }

    /* Reset device */
    gpio_set_level(cs_pin, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &(uint8_t){ADS1261_CMD_RESET},
    };
    spi_device_transmit(device->spi_handle, &t);
    gpio_set_level(cs_pin, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "ADS1261 initialized");
    return ESP_OK;
}

esp_err_t ads1261_write_register(ads1261_t *device, uint8_t reg, uint8_t value)
{
    if (!device) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_data[3] = {
        ADS1261_CMD_WREG | reg,  // Command + register address
        0x00,                     // Number of registers - 1
        value                     // Register value
    };

    gpio_set_level(device->cs_pin, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx_data,
    };
    esp_err_t ret = spi_device_transmit(device->spi_handle, &t);

    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(device->cs_pin, 1);

    return ret;
}

esp_err_t ads1261_read_register(ads1261_t *device, uint8_t reg, uint8_t *value)
{
    if (!device || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_data[2] = {
        ADS1261_CMD_RREG | reg,  // Command + register address
        0x00,                     // Number of registers - 1
    };
    uint8_t rx_data[3] = {0};

    gpio_set_level(device->cs_pin, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    esp_err_t ret = spi_device_transmit(device->spi_handle, &t);

    *value = rx_data[2];

    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(device->cs_pin, 1);

    return ret;
}

esp_err_t ads1261_set_mux(ads1261_t *device, uint8_t muxp, uint8_t muxn)
{
    uint8_t inpmux = (muxp << 4) | muxn;
    return ads1261_write_register(device, ADS1261_REG_INPMUX, inpmux);
}

esp_err_t ads1261_set_pga(ads1261_t *device, uint8_t gain)
{
    return ads1261_write_register(device, ADS1261_REG_PGA, (gain << 4) | 0x01);
}

esp_err_t ads1261_set_datarate(ads1261_t *device, uint8_t datarate)
{
    return ads1261_write_register(device, ADS1261_REG_DATARATE, datarate);
}

esp_err_t ads1261_set_ref(ads1261_t *device, uint8_t refsel)
{
    return ads1261_write_register(device, ADS1261_REG_REF, (refsel << 5) | 0x00);
}

esp_err_t ads1261_start_conversion(ads1261_t *device)
{
    if (!device) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(device->cs_pin, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    uint8_t cmd = ADS1261_CMD_START;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t ret = spi_device_transmit(device->spi_handle, &t);

    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(device->cs_pin, 1);

    return ret;
}

esp_err_t ads1261_read_adc(ads1261_t *device, int32_t *value)
{
    if (!device || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Wait for DRDY pin to go low */
    uint32_t timeout = ADS1261_TIMEOUT;
    while (gpio_get_level(device->drdy_pin) && timeout--) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    if (timeout == 0) {
        ESP_LOGE(TAG, "ADC read timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint8_t cmd = ADS1261_CMD_RDATA;
    uint8_t rx_data[4] = {0};

    gpio_set_level(device->cs_pin, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = &cmd,
        .rx_buffer = rx_data,
    };
    esp_err_t ret = spi_device_transmit(device->spi_handle, &t);

    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(device->cs_pin, 1);

    /* Combine 24-bit ADC value (MSB first) */
    *value = (int32_t)((rx_data[1] << 16) | (rx_data[2] << 8) | rx_data[3]);

    /* Sign extend 24-bit value to 32-bit */
    if (*value & 0x800000) {
        *value |= 0xFF000000;
    }

    return ret;
}

void ads1261_deinit(ads1261_t *device)
{
    if (device && device->spi_handle) {
        spi_bus_remove_device(device->spi_handle);
    }
}
