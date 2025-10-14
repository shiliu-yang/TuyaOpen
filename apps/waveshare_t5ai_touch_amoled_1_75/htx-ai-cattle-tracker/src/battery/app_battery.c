/**
 * @file app_battery.c
 * @brief app_battery module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_battery.h"

#include "tal_api.h"

#include "tkl_adc.h"
#include "tkl_gpio.h"

#include "app_dp.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define GET_BATTERY_TIME_MS          (5 * 60 * 1000) // 5 minutes
#define BATTERY_CHARGE_CHECK_TIME_MS (1500)          // 1.5 seconds

// for T5AI+4G
#define ADC_BATTERY_CAP_PIN    TUYA_GPIO_NUM_23
#define ADC_BATTERY_CHANNEL    3
#define ADC_BATTERY_CHARGE_PIN TUYA_GPIO_NUM_20

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
// extern bool app_check_network_ready(void);

void __battery_charge_pin_init(void);
void __battery_charge_pin_deinit(void);

/***********************************************************
***********************variable define**********************
***********************************************************/
static TUYA_ADC_BASE_CFG_T sg_adc_cfg = {
    .ch_list.data = 1 << ADC_BATTERY_CHANNEL,
    .ch_nums = 1, // adc Number of channel lists
    .width = 12,
    .mode = TUYA_ADC_CONTINUOUS,
    .type = TUYA_ADC_INNER_SAMPLE_VOL,
    .conv_cnt = 1,
};

static TIMER_ID sg_battery_timer_id = NULL;
static TIMER_ID sg_charge_check_timer_id = NULL;

volatile static bool sg_is_charging = false;

static uint8_t sg_battery_percentage = 50;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __charge_check_timer_cb(TIMER_ID timer_id, void *arg)
{
    TUYA_GPIO_LEVEL_E read_level = 0;
    bool prev_charging_state = sg_is_charging;

    tkl_gpio_read(ADC_BATTERY_CHARGE_PIN, &read_level);

    // charge pin is low when charging
    sg_is_charging = (read_level == TUYA_GPIO_LEVEL_LOW) ? true : false;

    // If charging state changed, trigger battery status update
    if (prev_charging_state != sg_is_charging) {
        PR_INFO("charging state changed: %s -> %s", prev_charging_state ? "charging" : "not charging",
                sg_is_charging ? "charging" : "not charging");
        if (sg_battery_timer_id) {
            tal_sw_timer_trigger(sg_battery_timer_id);
        }
    }
}

static void __battery_status_process(void)
{
    OPERATE_RET rt = OPRT_OK;
    int32_t battery_value = 0;

    if (sg_is_charging) {
        PR_INFO("battery is charging");
        // TODO:
        app_dp_battery_upload(sg_is_charging, sg_battery_percentage);
        return;
    }

    TUYA_CALL_ERR_LOG(tkl_adc_read_voltage(TUYA_ADC_NUM_0, &battery_value, 1));
    if (OPRT_OK != rt) {
        PR_ERR("read battery adc failed");
        return;
    }

    PR_INFO("battery voltage: %d mV", battery_value);

    // WAIT todo convert voltage to percentage
    // sg_battery_percentage = 50;

    // update dp
    app_dp_battery_upload(sg_is_charging, sg_battery_percentage);

    return;
}

static void __battery_timer_cb(TIMER_ID timer_id, void *arg)
{
    PR_INFO("--- battery timer callback");
    __battery_status_process();
    return;
}

void __battery_charge_pin_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_LEVEL_E read_level = 0;

    TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
    };
    rt = tkl_gpio_init(ADC_BATTERY_CHARGE_PIN, &in_pin_cfg);
    if (OPRT_OK != rt) {
        return;
    }
    tkl_gpio_read(ADC_BATTERY_CHARGE_PIN, &read_level);
    PR_DEBUG("battery charge pin level: %d", read_level);

    // charge pin is low when charging
    sg_is_charging = (read_level == TUYA_GPIO_LEVEL_LOW) ? true : false;
    PR_DEBUG("battery is %s", sg_is_charging ? "charging" : "not charging");

    return;
}

void __battery_charge_pin_deinit(void)
{
    tkl_gpio_deinit(ADC_BATTERY_CHARGE_PIN);
    return;
}

OPERATE_RET app_battery_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("battery init");
    __battery_charge_pin_init();

    TUYA_CALL_ERR_RETURN(tkl_adc_init(TUYA_ADC_NUM_0, &sg_adc_cfg));

    // Create battery status timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__battery_timer_cb, NULL, &sg_battery_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_battery_timer_id, GET_BATTERY_TIME_MS, TAL_TIMER_CYCLE));

    // Create charge check timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__charge_check_timer_cb, NULL, &sg_charge_check_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_charge_check_timer_id, BATTERY_CHARGE_CHECK_TIME_MS, TAL_TIMER_CYCLE));

    return rt;
}

OPERATE_RET app_battery_status_refresh(void)
{
    if (NULL == sg_battery_timer_id) {
        PR_ERR("battery module not init");
        return OPRT_COM_ERROR;
    }
    tal_sw_timer_trigger(sg_battery_timer_id);
    return OPRT_OK;
}

OPERATE_RET app_battery_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Stop and delete charge check timer
    if (sg_charge_check_timer_id) {
        tal_sw_timer_stop(sg_charge_check_timer_id);
        tal_sw_timer_delete(sg_charge_check_timer_id);
        sg_charge_check_timer_id = NULL;
    }

    // Stop and delete battery timer
    if (sg_battery_timer_id) {
        tal_sw_timer_stop(sg_battery_timer_id);
        tal_sw_timer_delete(sg_battery_timer_id);
        sg_battery_timer_id = NULL;
    }

    // Deinit charge pin and ADC
    __battery_charge_pin_deinit();
    tkl_adc_deinit(TUYA_ADC_NUM_0);

    return rt;
}
