/*****************************************************************************
 * | File      	:   dev_config.c
 * | Author      :   Waveshare team
 * | Function    :   Hardware underlying interface
 * | Info        :
 *----------------
 * |	This version:   V1.0
 * | Date        :   2025-08-29
 * | Info        :   Basic version
 *
 ******************************************************************************/
#include "dev_config.h"
#include "tal_api.h"

TDL_BUTTON_HANDLE button_hdl = NULL;

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
    } break;

    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
        dev_digital_write(EXAMPLE_SYS_EN_PIN, TUYA_GPIO_LEVEL_LOW);
    } break;

    default:
        break;
    }
}

OPERATE_RET dev_gpio_init(uint8_t pin, uint8_t mode)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    if (mode == 0 || mode == TUYA_GPIO_INPUT) {
        pin_cfg.mode = TUYA_GPIO_PULLUP;
        pin_cfg.direct = TUYA_GPIO_INPUT;
    } else {
        pin_cfg.mode = TUYA_GPIO_PULLUP;
        pin_cfg.direct = TUYA_GPIO_OUTPUT;
        pin_cfg.level = TUYA_GPIO_LEVEL_LOW;
    }
    return tkl_gpio_init(pin, &pin_cfg);
}

OPERATE_RET dev_digital_write(uint8_t pin, uint8_t value)
{
    return tkl_gpio_write(pin, value);
}

OPERATE_RET dev_digital_read(uint8_t pin, uint8_t *value)
{
    return tkl_gpio_read(pin, value);
}

OPERATE_RET dev_button_init(uint8_t pin)
{
    OPERATE_RET rt = OPRT_OK;

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = pin,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(EXAMPLE_PWR_BUTTON_NAME, &button_hw_cfg));

    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};

    TUYA_CALL_ERR_RETURN(tdl_button_create(EXAMPLE_PWR_BUTTON_NAME, &button_cfg, &button_hdl));

    return rt;
}

void dev_button_event_register(TDL_BUTTON_TOUCH_EVENT_E event, TDL_BUTTON_EVENT_CB cb)
{
    tdl_button_event_register(button_hdl, event, cb);
}

OPERATE_RET dev_sys_init()
{
    // 上电时，自动使能供电引脚
    OPERATE_RET rt = dev_gpio_init(EXAMPLE_SYS_EN_PIN, TUYA_GPIO_OUTPUT);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize GPIO (error: %d)", rt);
        return rt;
    }

    rt = dev_digital_write(EXAMPLE_SYS_EN_PIN, TUYA_GPIO_LEVEL_HIGH);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to en PWR (error: %d)", rt);
        return rt;
    }

    // 配置长按关机按键
    rt = dev_button_init(EXAMPLE_SYS_PWR_PIN);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to init pwr button (error: %d)", rt);
        return rt;
    }
    dev_button_event_register(TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    dev_button_event_register(TDL_BUTTON_LONG_PRESS_START, __button_function_cb);

    return rt;
}

OPERATE_RET dev_i2c_init()
{
    TUYA_IIC_BASE_CFG_T cfg;

    // GPS uses I2C Port 0 (GPIO 20/21) - exactly like working demo
    tkl_io_pinmux_config(GPS_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(GPS_I2C_SDA_PIN, TUYA_IIC0_SDA);

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    OPERATE_RET ret = tkl_i2c_init(TUYA_I2C_NUM_0, &cfg);
    if (OPRT_OK != ret) {
        PR_ERR("GPS I2C Port 0 init fail, err<%d>!", ret);
    } else {
        PR_INFO("GPS I2C Port 0 initialized on GPIO %d/%d", GPS_I2C_SCL_PIN, GPS_I2C_SDA_PIN);
    }
    return ret;
}

// Standard I2C functions - same as working GPS demo

OPERATE_RET dev_i2c_write(uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, data, 2, FALSE);
}

OPERATE_RET dev_i2c_write_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len)
{
    return tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

OPERATE_RET dev_i2c_read_nbytes(uint8_t addr, uint8_t reg, uint8_t *pdata, uint32_t len)
{
    tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, &reg, 1, FALSE);
    return tkl_i2c_master_receive(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

OPERATE_RET dev_i2c_read_only_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len)
{
    return tkl_i2c_master_receive(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

/**
 * @brief Initialize BMM150 I2C port (Port 2 with GPIO 14/15)
 */
OPERATE_RET bmm150_i2c_port_init()
{
    OPERATE_RET ret = OPRT_OK;

    // Configure pinmux for BMM150 I2C (Port 2)
    tkl_io_pinmux_config(BMM150_I2C_SCL_PIN_NUM, TUYA_IIC2_SCL);
    tkl_io_pinmux_config(BMM150_I2C_SDA_PIN_NUM, TUYA_IIC2_SDA);

    // Initialize I2C Port 2
    TUYA_IIC_BASE_CFG_T cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
        .speed = TUYA_IIC_BUS_SPEED_100K // BMM150 supports up to 400kHz
    };

    ret = tkl_i2c_init(TUYA_I2C_NUM_2, &cfg);
    if (ret != OPRT_OK) {
        PR_ERR("BMM150 I2C init failed: %d", ret);
        return ret;
    }

    PR_INFO("BMM150 I2C initialized on Port 2 (GPIO14/15)");
    return OPRT_OK;
}

/**
 * @brief Write BMM150 register via I2C Port 2
 */
OPERATE_RET bmm150_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t send_buf[2] = {reg, value};
    return tkl_i2c_master_send(TUYA_I2C_NUM_2, addr, send_buf, 2, TRUE);
}

/**
 * @brief Read BMM150 register via I2C Port 2
 */
OPERATE_RET bmm150_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length)
{
    OPERATE_RET ret = OPRT_OK;

    // Send register address
    ret = tkl_i2c_master_send(TUYA_I2C_NUM_2, addr, &reg, 1, FALSE);
    if (ret != OPRT_OK) {
        return ret;
    }

    // Read data
    return tkl_i2c_master_receive(TUYA_I2C_NUM_2, addr, buffer, length, TRUE);
}

#if 1
OPERATE_RET dev_uart_init(TUYA_UART_NUM_E port, uint32_t baudrate)
{
    OPERATE_RET ret;

    TUYA_UART_BASE_CFG_T cfg = {0};
    cfg.baudrate = baudrate;
    cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;

    ret = tkl_uart_init(port, &cfg);
    if (OPRT_OK != ret) {
        PR_ERR("UART init fail, err<%d>!", ret);
    } else {
        PR_INFO("UART port %d initialized at baudrate %d", port, baudrate);
    }

    return ret;
}

OPERATE_RET dev_uart_deinit(TUYA_UART_NUM_E port)
{
    return tkl_uart_deinit(port);
}

int dev_uart_write(TUYA_UART_NUM_E port, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        return -1;
    }
    return tkl_uart_write(port, (void *)data, len);
}

int dev_uart_read(TUYA_UART_NUM_E port, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    int total_read = 0;

    if (!data || len == 0) {
        return -1;
    }

    uint32_t start_time = tal_system_get_millisecond();
    uint32_t last_fifo_len = 0;
    last_fifo_len = tkl_uart_get_rxfifo_len(port);

    // wait data recv finish, fifo len no change in 50ms
    while (1) {
        uint32_t current_fifo_len = tkl_uart_get_rxfifo_len(port);
        if (current_fifo_len != last_fifo_len) {
            last_fifo_len = current_fifo_len;
            start_time = tal_system_get_millisecond();
        } else {
            if ((tal_system_get_millisecond() - start_time) > 50) {
                break;
            }
        }

        if ((tal_system_get_millisecond() - start_time) > timeout_ms) {
            PR_WARN("timeout not get data");
            break;
        }
        tal_system_sleep(10);
    }

    // Read available data
    uint32_t to_read_len = ((last_fifo_len > len) ? len : last_fifo_len);
    int bytes = tkl_uart_read(port, data, to_read_len);
    if (bytes < 0) {
        total_read = 0;
    } else {
        total_read = bytes;
    }

    return total_read;
}
#else
OPERATE_RET dev_uart_init(TUYA_UART_NUM_E port, uint32_t baudrate)
{
    OPERATE_RET rt;

    // Configure UART parameters using TAL (Tuya Abstraction Layer)
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = baudrate;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;
    cfg.rx_buffer_size = 1024; // Buffer size for receiving
    cfg.open_mode = O_BLOCK;   // Blocking mode

    rt = tal_uart_init(port, &cfg);
    if (OPRT_OK != rt) {
        PR_ERR("UART init fail, err<%d>!", rt);
    } else {
        PR_INFO("UART port %d initialized at baudrate %d", port, baudrate);
    }

    return rt;
}

OPERATE_RET dev_uart_deinit(TUYA_UART_NUM_E port)
{
    return tal_uart_deinit(port);
}

int dev_uart_write(TUYA_UART_NUM_E port, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        return -1;
    }
    return tal_uart_write(port, (void *)data, len);
}

int dev_uart_read(TUYA_UART_NUM_E port, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return tal_uart_read(port, data, len);
    if (!data || len == 0) {
        return -1;
    }

    // Use TAL UART read with timeout via polling
    uint32_t start_time = tal_system_get_millisecond();
    int total_read = 0;

    while (total_read < len) {
        int bytes = tal_uart_read(port, data + total_read, len - total_read);
        if (bytes > 0) {
            total_read += bytes;
            // If we got some data, return immediately (non-greedy read)
            break;
        }

        // Check timeout
        uint32_t elapsed = tal_system_get_millisecond() - start_time;
        if (elapsed >= timeout_ms) {
            break;
        }

        // Short delay to avoid busy waiting
        tal_system_sleep(10);
    }

    return total_read;
}
#endif