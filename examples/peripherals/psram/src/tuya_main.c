#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "tkl_timer.h"

#define HW_TIMER_ID TUYA_TIMER_NUM_0

#define BUF_SIZE 1024 * 1024 // 1MB

static void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    // init us timer
    TUYA_TIMER_BASE_CFG_T timer_cfg = {.mode = TUYA_TIMER_MODE_PERIOD, .args = NULL, .cb = NULL};
    TUYA_CALL_ERR_LOG(tkl_timer_init(HW_TIMER_ID, &timer_cfg));
    TUYA_CALL_ERR_LOG(tkl_timer_start(HW_TIMER_ID, 0xFFFFFFFF));

    uint32_t start_time_us = 0;
    uint32_t end_time_us = 0;

    PR_DEBUG("PSRAM test");

    // write test
    uint8_t *psram_buf = (uint8_t *)tal_psram_malloc(BUF_SIZE);
    if (psram_buf == NULL) {
        PR_ERR("psram malloc failed");
        return;
    }

    tkl_timer_get_current_value(HW_TIMER_ID, &start_time_us);
    for (size_t i = 0; i < BUF_SIZE; i++) {
        psram_buf[i] = i & 0xFF;
    }
    tkl_timer_get_current_value(HW_TIMER_ID, &end_time_us);
    PR_DEBUG("PSRAM write test cost %d - %d = %d us", end_time_us, start_time_us, end_time_us - start_time_us);
    double mbps = (double)BUF_SIZE / (1024 * 1024) / ((end_time_us - start_time_us) / 1000000.0);
    PR_DEBUG("PSRAM write speed: %.2f MB/s", mbps);

    // read test
    volatile uint32_t sum = 0;
    tkl_timer_get_current_value(HW_TIMER_ID, &start_time_us);
    for (size_t i = 0; i < BUF_SIZE; i++) {
        sum += psram_buf[i];
    }
    tkl_timer_get_current_value(HW_TIMER_ID, &end_time_us);
    PR_DEBUG("PSRAM read test cost %d - %d = %d us", end_time_us, start_time_us, end_time_us - start_time_us);
    mbps = (double)BUF_SIZE / (1024 * 1024) / ((end_time_us - start_time_us) / 1000000.0);
    PR_DEBUG("PSRAM read speed: %.2f MB/s", mbps);
    PR_DEBUG("sum: %u", sum);

    // memcpy test
    uint8_t *psram_buf2 = (uint8_t *)tal_psram_malloc(BUF_SIZE);
    if (psram_buf2 == NULL) {
        PR_ERR("psram malloc failed");
        tal_psram_free(psram_buf);
        return;
    }
    // memcpy 4KB test
    tkl_timer_get_current_value(HW_TIMER_ID, &start_time_us);
    for (size_t i = 0; i < BUF_SIZE; i += 4096) {
        memcpy((void *)(psram_buf2 + i), (void *)(psram_buf + i), 4096);
    }
    tkl_timer_get_current_value(HW_TIMER_ID, &end_time_us);
    PR_DEBUG("PSRAM memcpy 4KB test cost %d - %d = %d us", end_time_us, start_time_us, end_time_us - start_time_us);
    mbps = (double)BUF_SIZE / (1024 * 1024) / ((end_time_us - start_time_us) / 1000000.0);
    PR_DEBUG("PSRAM memcpy 4KB speed: %.2f MB/s", mbps);

    // memcpy 32KB test
    tkl_timer_get_current_value(HW_TIMER_ID, &start_time_us);
    for (size_t i = 0; i < BUF_SIZE; i += 32768) {
        memcpy((void *)(psram_buf2 + i), (void *)(psram_buf + i), 32768);
    }
    tkl_timer_get_current_value(HW_TIMER_ID, &end_time_us);
    PR_DEBUG("PSRAM memcpy 32KB test cost %d - %d = %d us", end_time_us, start_time_us, end_time_us - start_time_us);
    mbps = (double)BUF_SIZE / (1024 * 1024) / ((end_time_us - start_time_us) / 1000000.0);
    PR_DEBUG("PSRAM memcpy 32KB speed: %.2f MB/s", mbps);

    tal_psram_free(psram_buf);
    tal_psram_free(psram_buf2);
#else
    PR_DEBUG("PSRAM not enabled");
#endif
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif