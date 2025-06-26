/**
 * @file test_pwm.c
 * @brief test_pwm module is used to 
 * @version 0.1
 * @date 2025-06-24
 */

#include "test_pwm.h"

#include "tal_log.h"
#include "tkl_pwm.h"
#include "tkl_pinmux.h"

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


/***********************************************************
***********************function define**********************
***********************************************************/

// static void pwm_simple_demo(void *param)
// {
//     OPERATE_RET rt = OPRT_OK;
//     UINT32_T frequency = PWM_FREQUENCY;
//     UINT32_T count = 0;

//     uint32_t pwm_chan = tkl_io_pin_to_func(36, TUYA_IO_TYPE_PWM);

//     /*pwm init*/
//     TUYA_PWM_BASE_CFG_T pwm_cfg = {
//         .duty = 5000, /* 1-10000 */
//         .frequency = 10000,
//         .polarity  = TUYA_PWM_NEGATIVE,
//     };

//     if (OPRT_OK != tkl_pwm_init(pwm_chan, &pwm_cfg)) {
//         bk_printf("error tkl_pwm_info_set\r\n");
//         return -1;
//     }

//     /*start PWM3*/
//     if (OPRT_OK != tkl_pwm_start(pwm_chan)) {
//         bk_printf("error tkl_pwm_info_set\r\n");
//         return -1;
//     }
//     bk_printf("PWM%d start\r\n", pwm_chan);

//     while (1) {
//         /*Frequency, duty cycle settings*/
//         pwm_cfg.frequency = frequency;
//         if (OPRT_OK != tkl_pwm_info_set(pwm_chan, &pwm_cfg)) {
//             bk_printf("error tkl_pwm_info_set\r\n");
//             return -1;
//         }

//         if (OPRT_OK != tkl_pwm_start(pwm_chan)) {
//             bk_printf("error tkl_pwm_info_set\r\n");
//             return -1;
//         }
//         bk_printf("pwm%d , frequency: %d\r\n", pwm_chan,  frequency);

//         tkl_system_sleep(2000);

//         /*close pwm*/
//         count++;
//         if(count >= 3) {
//             bk_printf("pwm%d test end\r\n", pwm_chan,  frequency);
//             break;
//         }
//         frequency = frequency+10000;

//     }

//     if (OPRT_OK != tkl_pwm_stop(pwm_chan)) {
//         bk_printf("error tkl_pwm_info_set\r\n");
//         return -1;
//     }

// __EXIT:
//     bk_printf("PWM task is finished, will delete");
//     return;
// }

static void test_pwm_start(uint8_t pwm_chan)
{
    /*pwm init*/
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = 5000, /* 1-10000 */
        .frequency = 10000,
        .polarity  = TUYA_PWM_NEGATIVE,
    };

    if (OPRT_OK != tkl_pwm_init(pwm_chan, &pwm_cfg)) {
        PR_ERR("error tkl_pwm_info_set");
        return;
    }

    /*start PWM3*/
    if (OPRT_OK != tkl_pwm_start(pwm_chan)) {
        PR_ERR("error tkl_pwm_info_set");
        return;
    }
}

void test_pwm(void)
{
    uint32_t pwm_chan = tkl_io_pin_to_func(18, TUYA_IO_TYPE_PWM);
    test_pwm_start(pwm_chan);

    pwm_chan = tkl_io_pin_to_func(24, TUYA_IO_TYPE_PWM);
    test_pwm_start(pwm_chan);
}
