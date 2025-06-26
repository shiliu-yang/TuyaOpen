/**
 * @file test_adc.c
 * @brief test_adc module is used to 
 * @version 0.1
 * @date 2025-06-24
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_adc.h"

#include "tkl_adc.h"
#include "tkl_pinmux.h"
#include "tkl_gpio.h"

#include "tal_log.h"
#include "tal_system.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define BAT_ADC_PIN 28
#define CHG_DET_PIN 38

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/

void test_adc(void)
{
    TUYA_ADC_BASE_CFG_T tkl_cfg;

    int bat_adc_chan = tkl_io_pin_to_func(BAT_ADC_PIN, TUYA_IO_TYPE_ADC);
    tkl_cfg.ch_list.data = BIT(bat_adc_chan & 0xFF);

    tkl_cfg.ch_nums = 1;
    tkl_cfg.type = TUYA_ADC_INNER_SAMPLE_VOL;
    tkl_cfg.width = 12;
    tkl_cfg.mode = TUYA_ADC_CONTINUOUS;
    tkl_cfg.conv_cnt = 8;
    tkl_adc_init(0, &tkl_cfg);


    // gpio
    TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_LOW
    };
    tkl_gpio_init(CHG_DET_PIN, &in_pin_cfg);

    uint32_t adc_tick = 0;

    while (1) {
        adc_tick++;

        if (adc_tick > 50) {
            adc_tick = 0;
            int buffer[8] = {0};
            size_t buffer_size = sizeof(buffer) / sizeof(buffer[0]);
            memset(buffer, 0, buffer_size);

            tkl_adc_read_voltage(0, buffer, 8);

            for (int i = 0; i < 8; i++) {
                PR_NOTICE("ADC value %d: %d", i, buffer[i]);
            }
        }

        static uint8_t last_status = 0xff;
        uint8_t status = 0;
        tkl_gpio_read(CHG_DET_PIN, &status);
        if (status != last_status) {
            last_status = status;
            PR_NOTICE("CHG_DET_PIN status changed: %s", status ? "HIGH" : "LOW");
        }

        tal_system_sleep(100); // sleep 1s
    }

    return;
}
