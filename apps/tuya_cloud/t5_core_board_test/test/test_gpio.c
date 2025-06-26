/**
 * @file test_gpio.c
 * @brief test_gpio module is used to 
 * @version 0.1
 * @date 2025-06-23
 */

#include "test_gpio.h"

#include "tal_log.h"

#include "tkl_gpio.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static uint8_t sg_gpio_left[] = {4, 5, 6, 7, 8, 9, 24, 27, 30, 31, 26, 25, 23, 22, 21}; // 0 is log tx
static uint8_t sg_gpio_right[] = {2, 12, 13, 15, 14, 16, 18, 19, 47, 46, 45, 44, 43, 42, 20}; // 17 

/***********************************************************
***********************function define**********************
***********************************************************/

void test_gpio(void)
{
    TUYA_GPIO_LEVEL_E read_level = 0;

    for (int i = 0; i < sizeof(sg_gpio_left) / sizeof(sg_gpio_left[0]); i++) {
        TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
            .mode = TUYA_GPIO_PULLUP,
            .direct = TUYA_GPIO_INPUT,
            .level = TUYA_GPIO_LEVEL_LOW
        };

        TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
            .mode = TUYA_GPIO_PUSH_PULL,
            .direct = TUYA_GPIO_OUTPUT,
            .level = TUYA_GPIO_LEVEL_HIGH
        };

        // left GPIOs are inputs, right GPIOs are outputs
        tkl_gpio_init(sg_gpio_right[i], &out_pin_cfg);
        tkl_gpio_write(sg_gpio_right[i], TUYA_GPIO_LEVEL_HIGH);

        tkl_gpio_init(sg_gpio_left[i], &in_pin_cfg);
        tkl_gpio_read(sg_gpio_left[i], &read_level);
        if (read_level != TUYA_GPIO_LEVEL_HIGH) {
            PR_ERR("GPIO %d read level error, expected HIGH, got %d", sg_gpio_left[i], read_level);
        }

        // Toggle the output GPIO
        tkl_gpio_write(sg_gpio_right[i], TUYA_GPIO_LEVEL_LOW);
        tkl_gpio_read(sg_gpio_left[i], &read_level);
        if (read_level != TUYA_GPIO_LEVEL_LOW) {
            PR_ERR("GPIO %d read level error, expected LOW, got %d", sg_gpio_left[i], read_level);
        }

        // PR_NOTICE("Input GPIO %d, Output GPIO %d successfully tested", sg_gpio_left[i], sg_gpio_right[i]);

        // right GPIOs are outputs, left GPIOs are inputs
        tkl_gpio_init(sg_gpio_left[i], &out_pin_cfg);
        tkl_gpio_write(sg_gpio_left[i], TUYA_GPIO_LEVEL_HIGH);

        tkl_gpio_init(sg_gpio_right[i], &in_pin_cfg);
        tkl_gpio_read(sg_gpio_right[i], &read_level);
        if (read_level != TUYA_GPIO_LEVEL_HIGH) {
            PR_ERR("GPIO %d read level error, expected HIGH, got %d", sg_gpio_right[i], read_level);
        }

        // Toggle the output GPIO
        tkl_gpio_write(sg_gpio_left[i], TUYA_GPIO_LEVEL_LOW);
        tkl_gpio_read(sg_gpio_right[i], &read_level);
        if (read_level != TUYA_GPIO_LEVEL_LOW) {
            PR_ERR("GPIO %d read level error, expected LOW, got %d", sg_gpio_right[i], read_level);
        }
    }

    PR_NOTICE("All GPIO tests completed successfully");

    return;
}

