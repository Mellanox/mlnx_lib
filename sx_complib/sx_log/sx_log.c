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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <complib/sx_log.h>

/***** LOCAL VARIABLES *****/

static sx_log_t log;
static int log_users_count = 0;

static const char *month_str[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

/***** LOCAL FUNCTIONS *****/

static uint32_t
get_time_stamp_sec(void)
{
	struct timeval tv;
	tv.tv_sec = (time_t)0;
	tv.tv_usec = 0L;
	gettimeofday(&tv, NULL);
	return tv.tv_sec;
}

/***** APIs *****/

int
sx_log_init(
		const boolean_t flush,
		FILE *log_file,
		sx_log_cb_t log_cb
		)
{
	log.flush = flush;

	if (!log_cb && !log_file) {
		if (log.init) {
			log_users_count++;
			return 0;
		}

		/* use stdout as default */
		log.log_file = stdout;
		log.log_cb = NULL;
		log_users_count++;

		sx_log(SX_LOG_INFO, "LOG", "Initializing SX log with STDOUT as output file.\n");
	} else if (log_file && !log_cb) {
		if (log.init) {
			if (log_file != log.log_file) {
				sx_log(SX_LOG_ERROR, "LOG", "SX log is already initialized with user "
				       "output file, please assign the same output file.\n");
				return -1;
			}

			log_users_count++;
			return 0;
		}

		log.log_file = log_file;
		log.log_cb = NULL;
		log_users_count++;

		sx_log(SX_LOG_INFO, "LOG", "Initializing SX log with user output file.\n");
	} else if (log_cb && !log_file) {
		if (log.init) {
			if (log_cb != log.log_cb) {
				sx_log(SX_LOG_ERROR, "LOG", "Log callback function is already initialized "
				       "with another callback, please assign the same callback function.\n");
				return -1;
			}

			log_users_count++;
			return 0;
		}

		log.log_file = NULL;
		log.log_cb = log_cb;
		log_users_count++;

		sx_log(SX_LOG_INFO, "LOG", "Initializing SX log with user callback function.\n");
	} else {
		if (log.init) {
			sx_log(SX_LOG_ERROR, "LOG", "Log file and user callback function are mutual exclusive.\n");
		} else {
			fprintf(stderr, "Log file and user callback function are mutual exclusive.\n");
		}
		return -1;
	}


	log.init = TRUE;

	return 0;
}

void
sx_log_close()
{
	if (log_users_count > 1) {
		log_users_count--;
		return;
	}
	if (log_users_count == 0) {
		printf("LOG was not initialized yet , please call sx_log_init before using the LOG utility\n");
		return;
	}

	/* last user - close log */
	printf("SX log last user - going to close\n");
	log_users_count--;
	if (log.log_file && (log.log_file != stdout)) {
		fclose(log.log_file);
		log.log_file = stdout;
	}

	printf("SX log was closed\n");
}

void
sx_log_vprint(
		const sx_log_severity_t severity,
		const char *module_name,
		const char *p_str,
		va_list args
		)
{
	char buffer[LOG_ENTRY_SIZE_MAX];
	struct tm result;
	time_t tim;

	if (log_users_count == 0) {
		printf("LOG was not initialized yet , please call sx_log_init before using the LOG utility\n");
		return;
	}

	vsnprintf(buffer, LOG_ENTRY_SIZE_MAX, p_str, args);

	tim = get_time_stamp_sec();
	localtime_r(&tim, &result);

	if (log.log_file) {
		int verbosity_level = 0;
		SEVERITY_LEVEL_TO_VERBOSITY_LEVEL(severity,verbosity_level);
		fprintf(log.log_file,
			"%s %02d %02d:%02d:%02d %s %s: %s",
			(result.tm_mon < 12 ? month_str[result.tm_mon] : "???"),
			result.tm_mday, result.tm_hour, result.tm_min,
			result.tm_sec,
			SX_VERBOSITY_LEVEL_STR(verbosity_level),
			module_name,
			buffer);

		/* flush log on errors too */
		if (log.flush || (severity == SX_LOG_ERROR)) {
			fflush(log.log_file);
		}
	} else {
		log.log_cb(severity, module_name, buffer);
	}

}

void
sx_log(
		const sx_log_severity_t severity,
		const char *module_name,
		const char *p_str,
		...
		)
{
	va_list args;

	if (log_users_count == 0) {
		printf("LOG was not initialized yet , please call sx_log_init before using the LOG utility\n");
		return;
	}

	va_start(args, p_str);
	sx_log_vprint(severity, module_name, p_str, args);
	va_end(args);
}

void
sx_log_params_get(
		boolean_t    *flush_p,
		FILE        **log_file,
		sx_log_cb_t  *log_cb
		)
{
	if (log_users_count == 0) {
		printf("LOG was not initialized yet , please call sx_log_init before using the LOG utility\n");
		return;
	}

	flush_p[0] = log.flush;
	log_file[0] = log.log_file;
	log_cb[0] = log.log_cb;
}

