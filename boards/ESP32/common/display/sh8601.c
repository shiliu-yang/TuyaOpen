/**
 * @file sh8601.c
 * @brief sh8601 module is used to 
 * @version 0.1
 * @date 2025-04-30
 */

#include "esp_lcd_sh8601.h"

#include "esp_err.h"
#include "esp_log.h"
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "sh8601"

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

#define EXAMPLE_PIN_NUM_LCD_CS GPIO_NUM_12
#define EXAMPLE_PIN_NUM_LCD_PCLK GPIO_NUM_11
#define EXAMPLE_PIN_NUM_LCD_DATA0 GPIO_NUM_4
#define EXAMPLE_PIN_NUM_LCD_DATA1 GPIO_NUM_5
#define EXAMPLE_PIN_NUM_LCD_DATA2 GPIO_NUM_6
#define EXAMPLE_PIN_NUM_LCD_DATA3 GPIO_NUM_7
#define EXAMPLE_PIN_NUM_LCD_RST GPIO_NUM_NC
#define DISPLAY_WIDTH 368
#define DISPLAY_HEIGHT 448
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
} LCD_CONFIG_T;

// static i2c_master_bus_handle_t codec_i2c_bus_ = NULL;
extern i2c_master_bus_handle_t codec_i2c_bus_;
esp_io_expander_handle_t io_expander = NULL;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static LCD_CONFIG_T lcd_config = {0};

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10}
};

/***********************************************************
***********************function define**********************
***********************************************************/

void InitializeTca9554(void) {
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(codec_i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander);
    if(ret != ESP_OK)
        ESP_LOGE(TAG, "TCA9554 create returned error");
    ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 |IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_OUTPUT);
    ret |= esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_INPUT);
    ESP_ERROR_CHECK(ret);
    ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 1);
    ESP_ERROR_CHECK(ret);
    vTaskDelay(pdMS_TO_TICKS(100));
    ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 0);
    ESP_ERROR_CHECK(ret);
    vTaskDelay(pdMS_TO_TICKS(300));
    ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1|IO_EXPANDER_PIN_NUM_2, 1);
    ESP_ERROR_CHECK(ret);
}

void InitializeSpi() {
    spi_bus_config_t buscfg = {0};
    buscfg.sclk_io_num = GPIO_NUM_11;
    buscfg.data0_io_num = GPIO_NUM_4;
    buscfg.data1_io_num = GPIO_NUM_5;
    buscfg.data2_io_num = GPIO_NUM_6;
    buscfg.data3_io_num = GPIO_NUM_7;
    // buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

static void InitializeCodecI2c(void) {
    // Initialize I2C peripheral
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_15,
        .scl_io_num = GPIO_NUM_14,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
}

void SetBrightnessImpl(uint8_t brightness) {
    lvgl_port_lock(0);
    // TODO: lock
    uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
    int lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    esp_lcd_panel_io_tx_param(lcd_config.panel_io, lcd_cmd, &data, sizeof(data));
    // TODO: unlock
    lvgl_port_unlock();
}


void __display_sh8601_init(void)
{
    // Initialize the display here
    ESP_LOGI(TAG, "SH8601 initialized");

    // InitializeCodecI2c();
    InitializeTca9554();
    InitializeSpi();

    esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        EXAMPLE_PIN_NUM_LCD_CS,
        NULL,
        NULL
    );
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &lcd_config.panel_io));

    ESP_LOGD(TAG, "Install LCD driver");
    const sh8601_vendor_config_t vendor_config = {
        .init_cmds = &vendor_specific_init[0],
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
        .flags ={
            .use_qspi_interface = 1,
        }
    };

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.flags.reset_active_high = 1,
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = (void *)&vendor_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(lcd_config.panel_io, &panel_config, &lcd_config.panel));

    esp_lcd_panel_reset(lcd_config.panel);
    esp_lcd_panel_init(lcd_config.panel);
    esp_lcd_panel_invert_color(lcd_config.panel, false);
    esp_lcd_panel_mirror(lcd_config.panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    ESP_LOGI(TAG, "Turning display on");
    esp_lcd_panel_disp_on_off(lcd_config.panel, true);

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = lcd_config.panel_io,
        .panel_handle = lcd_config.panel,
        .control_handle = NULL,
        .buffer_size = DISPLAY_WIDTH * 10,
        .double_buffer = false,
        .trans_size = 0,
        .hres = DISPLAY_WIDTH,
        .vres = DISPLAY_HEIGHT,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 1,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    lv_disp_t *disp = lvgl_port_add_disp(&display_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    SetBrightnessImpl(80);
}
