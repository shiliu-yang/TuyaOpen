/**
 * @file at_module_ml307r.h
 * @brief at_module_ml307r module is used to
 * @version 0.1
 * @date 2025-11-13
 *
 */

#ifndef __AT_MODULE_ML307R_H__
#define __AT_MODULE_ML307R_H__

#include "tuya_cloud_types.h"
#include "at_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief initialize the ml307r module
 *
 * @param ops: the operations of the ml307r module
 * @param cb: the callback of the ml307r module
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET at_module_ml307r_init(AT_MODULE_OPS_T *ops, AT_MODULE_CB cb);

/**
 * @brief deinitialize the ml307r module
 *
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET at_module_ml307r_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __AT_MODULE_ML307R_H__ */
