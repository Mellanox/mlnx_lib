/*
 * Copyright (c) 2004-2006 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __SX_LOG_H__
#define __SX_LOG_H__

#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <complib/cl_types.h>

#ifndef __MODULE__
#define __MODULE__ LOG
#endif

#define LOG_ENTRY_SIZE_MAX 256

typedef uint8_t sx_log_level_t;
#define QUOTEME_(x) #x						/* add "" to x */
#define QUOTEME(x) QUOTEME_(x)

#define INTERNAL_LOG_VAR_NAME(MODULE) sx_##MODULE##_verb_level	/* build a variable name from sx_xxx_verb_leve && MODULE */
#define LOG_VAR_NAME(MODULE) INTERNAL_LOG_VAR_NAME(MODULE)

/***** VERBOSITY LEVEL *****/

/**
 * sx_verbosity_level_t enumerated type is used to store
 * the Log level of a specific module
 */
typedef	enum sx_verbosity_level {
	SX_VERBOSITY_LEVEL_NONE = 0,				/**<! Do not print log messages 		  */
	SX_VERBOSITY_LEVEL_ERROR,				/**<! Print only error messages 		  */
	SX_VERBOSITY_LEVEL_WARNING,				/**<! Print only warning messages 		  */
	SX_VERBOSITY_LEVEL_NOTICE,				/**<! Print only notice messages 		  */
	SX_VERBOSITY_LEVEL_INFO,				/**<! Print only information messages 		  */
	SX_VERBOSITY_LEVEL_DEBUG,				/**<! Print only debug messages 		  */
	SX_VERBOSITY_LEVEL_FUNCS,				/**<! Print only functions messages 		  */
	SX_VERBOSITY_LEVEL_FRAMES,				/**<! Print only frames messages 		  */
	SX_VERBOSITY_LEVEL_ALL, 				/**<! Print all messages 			  */
	SX_VERBOSITY_LEVEL_MIN = SX_VERBOSITY_LEVEL_NONE,	/**<! Minimum verbosity level 			  */
	SX_VERBOSITY_LEVEL_MAX = SX_VERBOSITY_LEVEL_ALL,	/**<! Maximum verbosity level 			  */
	SX_VERBOSITY_LEVEL_MGMT_MIN = SX_VERBOSITY_LEVEL_NONE,	/**<! Minimum verbosity level for managment layer */
	SX_VERBOSITY_LEVEL_MGMT_MAX = SX_VERBOSITY_LEVEL_DEBUG	/**<! Minimum verbosity level for managment layer */
} sx_verbosity_level_t;

#define SX_VERBOSITY_LEVEL_CHECK_RANGE(verbosity) ((verbosity <= SX_VERBOSITY_LEVEL_MAX) && ((int)verbosity >= SX_VERBOSITY_LEVEL_MIN))

static const char *sx_verbosity_level_name[] = {
	"NONE   ",
	"ERROR  ",
	"WARN   ",
	"NOTICE ",
	"INFO   ",
	"DEBUG  ",
	"FUNCS  ",
	"FRAMES ",
	"ALL    "
};

static const int sx_verbosity_level_name_len = sizeof(sx_verbosity_level_name)/sizeof(char*);

#define SX_VERBOSITY_LEVEL_STR(index) (((int)index >= 0) && ((int)index < (int)sx_verbosity_level_name_len)?sx_verbosity_level_name[index]:"UNKNOWN")

/***** VERBOSITY TARGET *****/

/**
 * sx_log_verbosity_target_t enumerated type is used to store
 * the type of the module it's verbosity is changed
 */
typedef enum sx_log_verbosity_target {
        SX_LOG_VERBOSITY_TARGET_API = 0,	/**< change only API verbosity level */
        SX_LOG_VERBOSITY_TARGET_MODULE = 1,	/**< change only Module verbosity level */
        SX_LOG_VERBOSITY_BOTH = 2,		/**< change both Module & API verbosity level */

        SX_LOG_VERBOSITY_MIN = SX_LOG_VERBOSITY_TARGET_API,	/**< Minimum value of verbosity target */
        SX_LOG_VERBOSITY_MAX = SX_LOG_VERBOSITY_BOTH		/**< Maximum value of verbosity target */
} sx_log_verbosity_target_t;

static const char *sx_verbosity_target_name[] = {
        "API",
        "MODULE",
        "BOTH"
};

static const int sx_verbosity_target_name_len = sizeof(sx_verbosity_target_name)/sizeof(char*);

#define SX_VERBOSITY_TARGET_STR(index) ((index >= 0) && (index < sx_verbosity_target_name_len)?sx_verbosity_target_name[index]:"UNKNOWN")

/***** severity LEVEL *****/

#define SX_LOG_NONE		(SX_VERBOSITY_LEVEL_NONE		)
#define SX_LOG_ERROR		((0x1 <<  SX_VERBOSITY_LEVEL_ERROR)   -1)
#define SX_LOG_WARNING		((0x1 <<  SX_VERBOSITY_LEVEL_WARNING) -1)
#define SX_LOG_NOTICE		((0x1 <<  SX_VERBOSITY_LEVEL_NOTICE)  -1)
#define SX_LOG_INFO		((0x1 <<  SX_VERBOSITY_LEVEL_INFO)    -1)
#define SX_LOG_DEBUG		((0x1 <<  SX_VERBOSITY_LEVEL_DEBUG)   -1)
#define SX_LOG_FUNCS		((0x1 <<  SX_VERBOSITY_LEVEL_FUNCS)   -1)
#define SX_LOG_FRAMES		((0x1 <<  SX_VERBOSITY_LEVEL_FRAMES)  -1)
#define SX_LOG_ALL		((0x1 <<  SX_VERBOSITY_LEVEL_ALL)     -1)

typedef uint32_t sx_log_severity_t;

#define SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(SEVERITY_LEVEL,VERBOSITY_LEVEL)	\
	do {									\
		int __value = SEVERITY_LEVEL+1;					\
		int __result = VERBOSITY_LEVEL;					\
		while (!(__value & 1))						\
		{								\
	   		__value >>= 1;						\
	   		++__result;						\
		}								\
		VERBOSITY_LEVEL = __result;					\
	} while(0)

#define VERBOSITY_LEVEL_TO_SEVERITY_LEVEL(VERBOSITY_LEVEL) ((0x1 <<  VERBOSITY_LEVEL)   -1)

#define VERBOSITY_LEVEL_TO_SYSLOG_LEVEL(VERBOSITY_LEVEL) (VERBOSITY_LEVEL + 2)

/**
 * sx_log_cb_t function type is used as a callback function for log prints in the user context.
 * sevirty : Message severity level
 * module name : Name of the module who initiated the message
 * msg : Log message
 */
typedef void (*sx_log_cb_t)(sx_log_severity_t severity, const char *module_name, char *msg);

typedef struct sx_log {
	boolean_t flush;
	FILE *log_file;
	sx_log_cb_t log_cb;
	boolean_t init;
} sx_log_t;

/***** APIs *****/

int
sx_log_init(
		const boolean_t flush,
		FILE *log_file,
		sx_log_cb_t log_cb
		);

void
sx_log_close(void);

void
sx_log(
		const sx_log_severity_t severity,
		const char *module_name,
		const char *p_str, ...) __attribute__((format(printf, 3, 4))
		);

void
sx_log_vprint(
		const sx_log_severity_t severity,
		const char *module_name,
		const char *p_str,
		va_list args
		);

void
sx_log_params_get(
		boolean_t    *flush_p,
		FILE        **log_file,
		sx_log_cb_t  *log_cb
		);

/***** LOG PRINTS MACROS *****/

#ifdef _DEBUG_

#define SX_LOG(level, fmt, arg...)								\
	do {											\
		uint32_t __verbosity_level = 0;							\
		SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(level, __verbosity_level);			\
		if (LOG_VAR_NAME(__MODULE__) >= __verbosity_level) { 			 	\
			sx_log(level, QUOTEME(__MODULE__), "%s[%d]- %s: " fmt,			\
				__FILE__, __LINE__, __FUNCTION__, ##arg);			\
		}										\
	} while (0)

#else /* _DEBUG_ */

#define SX_LOG(level, fmt, arg...)								\
	do {											\
		uint32_t __verbosity_level_in = 0;						\
		SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(level, __verbosity_level_in);			\
		if (LOG_VAR_NAME(__MODULE__) >= __verbosity_level_in) {				\
			if (__verbosity_level_in >= SX_VERBOSITY_LEVEL_DEBUG) {			\
				sx_log(level, QUOTEME(__MODULE__), "%s[%d]- %s: " fmt,		\
					__FILE__, __LINE__, __FUNCTION__, ##arg);		\
			} else {								\
				sx_log(level, QUOTEME(__MODULE__), fmt, ##arg);			\
			}									\
		}										\
	} while (0)

#endif	/*	_DEBUG_	*/


#define SX_LOG_ENTER()		SX_LOG(SX_LOG_FUNCS, "%s: [\n", __FUNCTION__)
#define SX_LOG_EXIT()		SX_LOG(SX_LOG_FUNCS, "%s: ]\n", __FUNCTION__)
#define SX_LOG_DBG(fmt, arg...)	SX_LOG(SX_LOG_DEBUG, fmt, ##arg)
#define SX_LOG_INF(fmt, arg...)	SX_LOG(SX_LOG_INFO, fmt, ##arg)
#define SX_LOG_WRN(fmt, arg...)	SX_LOG(SX_LOG_WARNING, fmt, ##arg)
#define SX_LOG_ERR(fmt, arg...)	SX_LOG(SX_LOG_ERROR, fmt, ##arg)
#define SX_LOG_NTC(fmt, arg...)	SX_LOG(SX_LOG_NOTICE, fmt, ##arg)

#define SX_HEXDUMP(level, ptr, len)						\
	do {									\
		char __buff[LOG_ENTRY_SIZE_MAX];				\
		uint8_t *__data = (uint8_t *)(ptr);				\
		uint32_t __i = 0, __pos = 0;					\
		uint32_t __verbosity_level = 0;					\
										\
		SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(level, __verbosity_level);	\
		if (LOG_VAR_NAME(__MODULE__) < __verbosity_level) { 		\
			break;							\
		}								\
		for (__i = 0; __i < len; ++__i) {				\
			if ((__i % 4) == 0) {					\
				if (__pos) {					\
					__buff[__pos] = '\0';			\
					SX_LOG(level, "%s\n", __buff);		\
					__pos = 0;				\
				}						\
				snprintf(__buff + __pos, 9, "0x%04X: ", __i);	\
				__pos += 8;					\
			}							\
			snprintf(__buff + __pos, 4, "%02hhX ", __data[__i]);	\
			__pos += 3;						\
		}								\
		if (__pos) {							\
			__buff[__pos] = '\0';					\
			SX_LOG(level, "%s\n", __buff);				\
		}								\
	} while (0)

#define SX_HEXDUMP16(level, ptr, len)						\
	do {									\
		char __buff[LOG_ENTRY_SIZE_MAX];				\
		uint8_t *__data = (uint8_t *)(ptr);				\
		uint32_t __i = 0, __pos = 0;					\
		uint32_t __verbosity_level = 0;					\
										\
		SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(level, __verbosity_level);	\
		if (LOG_VAR_NAME(__MODULE__) < __verbosity_level) { 		\
			break;							\
		}								\
		for (__i = 0; __i < len; ++__i) {				\
			if ((__i % 16) == 0) {					\
				if (__pos) {					\
					__buff[__pos] = '\0';			\
					SX_LOG(level, "%s\n", __buff);		\
					__pos = 0;				\
				}						\
				snprintf(__buff + __pos, 9, "0x%04X: ", __i);	\
				__pos += 8;					\
			} else if ((__i % 8) == 0) {				\
				snprintf(__buff + __pos, 2, " ");		\
				__pos += 1;					\
			}							\
			snprintf(__buff + __pos, 4, "%02hhX ", __data[__i]);	\
			__pos += 3;						\
		}								\
		if (__pos) {							\
			__buff[__pos] = '\0';					\
			SX_LOG(level, "%s\n", __buff);				\
		}								\
	} while (0)

#endif /* __SX_LOG_H__ */
