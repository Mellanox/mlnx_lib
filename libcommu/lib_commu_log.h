/*
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2014 ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of Mellanox Technologies, Ltd.
 * (the "Company") and all right, title, and interest in and to the software product,
 * including all associated intellectual property rights, are and shall
 * remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#ifndef LIB_COMMU_LOG_H_
#define LIB_COMMU_LOG_H_

#include <complib/sx_log.h>

#ifdef LIB_COMMU_LOG_C_
/************************************************
 *  Local Defines
 ***********************************************/

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

#endif

/************************************************
 *  Defines
 ***********************************************/

#define INIT_ERR_MSG "Library isn't initialized"

/************************************************
 *  Macros
 ***********************************************/
/* Temporary using sx log as our logging utility
   In the future need to replace this with different
   logging mechanism */

#define LCM_LOG(level, fmt, arg ...) SX_LOG(level, fmt, ## arg)

/************************************************
 *  Type definitions
 ***********************************************/

typedef sx_log_cb_t lib_commu_log_cb_t;

enum lib_commu_verbosity_level {
    LCOMMU_VERBOSITY_LEVEL_NONE = SX_VERBOSITY_LEVEL_NONE,
    LCOMMU_VERBOSITY_LEVEL_ERROR = SX_VERBOSITY_LEVEL_ERROR,
    LCOMMU_VERBOSITY_LEVEL_WARNING = SX_VERBOSITY_LEVEL_WARNING,
    LCOMMU_VERBOSITY_LEVEL_NOTICE = SX_VERBOSITY_LEVEL_NOTICE,
    LCOMMU_VERBOSITY_LEVEL_INFO = SX_VERBOSITY_LEVEL_INFO,
    LCOMMU_VERBOSITY_LEVEL_DEBUG = SX_VERBOSITY_LEVEL_DEBUG,
    LCOMMU_VERBOSITY_LEVEL_FUNCS = SX_VERBOSITY_LEVEL_FUNCS,
    LCOMMU_VERBOSITY_LEVEL_ALL,
    LCOMMU_VERBOSITY_LEVEL_MIN = LCOMMU_VERBOSITY_LEVEL_NONE,
    LCOMMU_VERBOSITY_LEVEL_MAX = LCOMMU_VERBOSITY_LEVEL_ALL,
};

enum lib_commu_log_levels {
    LCOMMU_LOG_NONE = SX_LOG_NONE,
    LCOMMU_LOG_ERROR = SX_LOG_ERROR,
    LCOMMU_LOG_WARNING = SX_LOG_WARNING,
    LCOMMU_LOG_NOTICE = SX_LOG_NOTICE,
    LCOMMU_LOG_INFO = SX_LOG_INFO,
    LCOMMU_LOG_DEBUG = SX_LOG_DEBUG,
    LCOMMU_LOG_FUNCS = SX_LOG_FUNCS
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function print to log
 *
 * @param[in] severity - The log severity
 * @param[in] module - module name
 * @param[in] format - string format
 *
 */

/*void libcommu_log(int severity, const char *module, char *format, ...); TODO*/

#endif /* LIB_COMMU_LOG_H_ */
