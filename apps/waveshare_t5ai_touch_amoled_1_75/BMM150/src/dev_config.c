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
    if(mode == 0 || mode == TUYA_GPIO_INPUT) {
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
        .mode = BUTTON_IRQ_MODE,
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
    //上电时，自动使能供电引脚
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

    //配置长按关机按键
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

    tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC0_SDA);

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    OPERATE_RET ret = tkl_i2c_init(TUYA_I2C_NUM_0, &cfg);
    if (OPRT_OK != ret) {
        PR_ERR("i2c init fail, err<%d>!", ret);
    }
    return ret;


}

OPERATE_RET dev_i2c_write(uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, data, 2, TRUE);
}

OPERATE_RET dev_i2c_write_nbytes(uint8_t addr, uint8_t *pdata, uint32_t len)
{
    return tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, pdata, len, FALSE);
}

OPERATE_RET dev_i2c_read_nbytes(uint8_t addr, uint8_t reg, uint8_t *pdata, uint32_t len)
{
    // Write register address first, then read
    OPERATE_RET ret = tkl_i2c_master_send(TUYA_I2C_NUM_0, addr, &reg, 1, TRUE);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    tal_system_sleep(10); // Delay for register access
    
    return tkl_i2c_master_receive(TUYA_I2C_NUM_0, addr, pdata, len, TRUE);
}

