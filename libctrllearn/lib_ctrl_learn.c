/* Copyright (c) 2014  Mellanox Technologies, Ltd. All rights reserved.
 *
 * This software is available to you under BSD license below:
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
 */ 

#include "lib_ctrl_learn.h"
#include "oes_status.h"
#include "oes_api_event.h"
#include "oes_api_fdb.h"
#include <complib/cl_thread.h>
#include <oes_types.h>
#include  "/usr/include/asm-generic/errno-base.h"

#include <complib/sx_log.h>
#include <complib/cl_qmap.h>
#include <complib/cl_pool.h>
#include <complib/cl_spinlock.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "lib_ctrl_learn_defs.h"
#include "lib_ctrl_learn_uc_db.h"
#include "lib_ctrl_learn_filters.h"
#include "lib_ctrl_learn_oes_errno_convertor.h"
#include "lib_ctrl_learn_hal.h"

/************************************************
 *  Local Defines
 ***********************************************/

/************************************************
 *  Local Macros
 ***********************************************/

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(PARAM) ((void)(PARAM))
#endif

#define QUOTEME_(x) #x                      /* add "" to x */
#define QUOTEME(x) QUOTEME_(x)

#define LOG CL_LOG
/************************************************
 *  Local definitions
 ***********************************************/
#undef  __MODULE__
#define __MODULE__ CTRL_LEARN

#define VID_MAX         (4094)

#define MAX_EVENT_INFO_SIZE   CTRL_LEARN_FDB_NOTIFY_SIZE_MAX

/* Simulate OES mode */
/*#define SIMULATE_MODE 0*/
/**
 * I/G bit mask For Validity check
 */
#define FDB_GROUP_ADDRESS_MASK  0x01

/************************************************
 *  Local Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static cl_thread_t ctrl_learn_thread;
static int ctrl_learn_br_id = 0;
static int is_ctrl_learn_initialized = 0;
static int quit_ctrl_learn_thread_fd[2];
static int simulate_oes_event_fd[2];
static int muliple_fdb_notif_enabled = 0;
static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) =
    SX_VERBOSITY_LEVEL_NOTICE;
static cl_event_t wait_for_start_event;

/* event info simualtion (for testing purpose) */
static struct oes_event_info event_info_sim[MAX_EVENT_INFO_SIZE];
static int event_num_sim = 0;

/* fdb lock */
static cl_spinlock_t ctrl_learn_fdb_lock;
/************************************************
 *  Local function declarations
 ***********************************************/

/*  This function flush the fdb according to specified key filter */
static int ctrl_learn_flush_db_by_filter(struct fdb_uc_key_filter* key_filter);

/* control learn thread function */
static void ctrl_learn_thread_routine(void *data);

static ctrl_learn_log_cb ctrl_learn_logging_cb = NULL;

/* notificatio callback for new/aged mac */
static ctrl_learn_notification_func ctrl_learn_notification_cb = NULL;

/* init deinit cookie callback */
static ctrl_learn_mac_addr_cookie_init_deinit_func ctrl_learn_init_deinit_cb =
    NULL;

/* This function flush the fdb */
static int ctrl_learn_handle_flush_all(void);

/* handle oes event */
int ctrl_learn_handle_oes_event( struct oes_event_info* event_info,
                                 int event_num);

/* lib ctrl learn verifies whether possible to add notified entries to fdb */
static int
update_approved_list(struct ctrl_learn_fdb_notify_data* notif_records);


static int is_ctrl_learn_start = 0;

static int ctrl_learn_stop_signal = 0;

/************************************************
 *  Function implementations
 ***********************************************/


/**
 *  This function set the module verbosity level
 *
 *  @param[in] verbosity_level - module verbosity level
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int
ctrl_learn_log_verbosity_set(int verbosity_level)
{
    int err = 0;

    LOG_VAR_NAME(__MODULE__) = verbosity_level;

    err = fdb_uc_db_filter_log_verbosity_set(verbosity_level);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed to set fdb uc filter db log verbosity err [%d]\n", err);
        return err;
    }

    err = fdb_uc_db_log_verbosity_set(verbosity_level);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed to set fdb uc db log verbosity err [%d]\n", err);
        return err;
    }

    return err;
}

/**
 *  This function get the log verbosity level of the module
 *
 *  @param[out] verbosity_level - module verbosity level
 *
 *  @return 0 when successful.
 *  @return -EINVAL - Unsupported verbosity_level
 */
int
ctrl_learn_log_verbosity_get(int * verbosity_level)
{
    int err = 0;

    *verbosity_level = LOG_VAR_NAME(__MODULE__);

    return err;
}

/**
 *  This function initialize the control learning library
 *
 *  @param[in] br_id - bridge id associated with the control learning
 *  @param[in] enable_multiple_fdb_notif - enable receive of multiple fdb notification mode
 *                                                                      ( 1 - enabled, 0 - disabled)
 *  @param[in] logging_cb - optional log messages callback
 *
 *  @return 0 when successful.
 *  @return -EPERM general error
 */
int
ctrl_learn_init(int br_id, int enable_multiple_fdb_notif,
                ctrl_learn_log_cb logging_cb)
{
    cl_status_t cl_err = CL_SUCCESS;
    int err = 0;

    ctrl_learn_br_id = br_id;
    ctrl_learn_logging_cb = logging_cb;

    /* Call fdb Init */

    cl_err = cl_event_init(&(wait_for_start_event), FALSE);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        return err;
    }

    err = fdb_uc_db_init(logging_cb);
    if (err != 0) {
        err = -EPERM;
        return err;
    }

    /* create termination for thread */
    if (pipe(quit_ctrl_learn_thread_fd) == -1) {
        err = -EPERM;
        return err;
    }

    /* create simulation fd */
    if (pipe(simulate_oes_event_fd) == -1) {
        err = -EPERM;
        return err;
    }

    cl_spinlock_init(&ctrl_learn_fdb_lock);

    /* initialize thread */
    cl_err = cl_thread_init(&ctrl_learn_thread, ctrl_learn_thread_routine,
                            NULL, NULL);
    if (cl_err != CL_SUCCESS) {
        err = -ENOMEM;
        LOG(CL_LOG_ERR,
            "Could not create Control Learning thread\n");
        goto out_pipe;
    }

    muliple_fdb_notif_enabled = enable_multiple_fdb_notif;

    goto out;

out_pipe:
    close(quit_ctrl_learn_thread_fd[0]);
    close(quit_ctrl_learn_thread_fd[1]);

    close(simulate_oes_event_fd[0]);
    close(simulate_oes_event_fd[1]);

    cl_spinlock_destroy(&ctrl_learn_fdb_lock);
out:

    is_ctrl_learn_initialized = 1;
    return 0;
}

/**
 *  This is a implementation of control learning thread
 *  The thread listen to OES FDB Event
 *
 * @param[in] data - pointer to data sent from cl_thread_init function
 *
 * @return void
 */
void
ctrl_learn_thread_routine(void *data)
{
    cl_status_t cl_err = CL_SUCCESS;
    oes_status_e oes_status = OES_STATUS_SUCCESS;
    int fd = 0;
    int vs_ext = 0;
    int event_recv_vs_ext = 0;
    int err = 0;
    fd_set input;
    int max_fd = 0;
    struct oes_event_info event_info[MAX_EVENT_INFO_SIZE];
    int event_num = 0;
    int bytes;

    UNUSED_PARAM(data);
    /* Wait for Start */

    /* Wait for start event */
    cl_err = cl_event_wait_on(&(wait_for_start_event), EVENT_NO_TIMEOUT, TRUE);
    if (cl_err != CL_SUCCESS) {
        LOG(CL_LOG_ERR,  "Failed at cl_event_wait_on cl_err [%d]\n",
            cl_err);
        return;
    }

    if (ctrl_learn_stop_signal == 1) {
        return;
    }

    oes_status = oes_api_event_fd_set(OES_ACCESS_CMD_CREATE, &fd, &vs_ext);
    if (oes_status != OES_STATUS_SUCCESS) {
        LOG(CL_LOG_ERR,
            "Failed at oes_api_event_fd_set oes_status [%d]\n", oes_status);
        return;
    }

    oes_status = oes_api_event_register_set(OES_ACCESS_CMD_ADD,
                                            ctrl_learn_br_id,
                                            OES_EVENT_ID_FDB, fd, &vs_ext);
    if (oes_status != OES_STATUS_SUCCESS) {
        LOG(CL_LOG_ERR,
            "Failed at oes_api_event_register_set oes_status [%d]\n",
            oes_status);
        return;
    }

    oes_status = oes_api_fdb_learn_mode_set(ctrl_learn_br_id,
                                            OES_FDB_CONTROL_LEARN, NULL);
    if (oes_status != OES_STATUS_SUCCESS) {
        LOG(CL_LOG_ERR,
            "Failed at oes_api_event_recv oes_status [%d]\n", oes_status);
        return;
    }

    is_ctrl_learn_start = 1;

    while (TRUE) {
        FD_ZERO(&input);
        FD_SET(quit_ctrl_learn_thread_fd[0], &input);
        FD_SET(simulate_oes_event_fd[0], &input);
        FD_SET(fd, &input);
        /* find largest fd */
        if ((fd >= quit_ctrl_learn_thread_fd[0]) &&
            (fd >= simulate_oes_event_fd[0])) {
            max_fd = fd + 1;
        }
        else if ((quit_ctrl_learn_thread_fd[0] >= fd) &&
                 (quit_ctrl_learn_thread_fd[0] >= simulate_oes_event_fd[0])) {
            max_fd = quit_ctrl_learn_thread_fd[0] + 1;
        }
        else {
            max_fd = simulate_oes_event_fd[0] + 1;
        }

        err = select(max_fd, &input, NULL, NULL, NULL);
        /* 0 return is an error, because we use timeout==NULL */
        if (err <= 0) {
            /* log for error */
            LOG(CL_LOG_ERR,
                "select failed err [%d]\n", err);
            return;
        }

        if (FD_ISSET(quit_ctrl_learn_thread_fd[0], &input)) {
            LOG(CL_LOG_DEBUG, "Terminating ctrl learn thread\n");

            read(quit_ctrl_learn_thread_fd[0], &bytes, sizeof(bytes));

            oes_status = oes_api_event_register_set(OES_ACCESS_CMD_DELETE,
                                                    ctrl_learn_br_id,
                                                    OES_EVENT_ID_FDB, fd,
                                                    &vs_ext);
            if (oes_status != OES_STATUS_SUCCESS) {
                LOG(CL_LOG_ERR,
                    "Failed at oes_api_event_register_set oes_status [%d]\n",
                    oes_status);
                return;
            }

            oes_status = oes_api_event_fd_set(OES_ACCESS_CMD_DESTROY, &fd,
                                              &vs_ext);
            if (oes_status != OES_STATUS_SUCCESS) {
                LOG(CL_LOG_ERR,
                    "Failed at oes_api_event_fd_set oes_status [%d]\n",
                    oes_status);
                return;
            }

            oes_status = oes_api_fdb_learn_mode_set(ctrl_learn_br_id,
                                                    OES_FDB_AUTO_LEARN, NULL);
            if (oes_status != OES_STATUS_SUCCESS) {
                LOG(CL_LOG_ERR,
                    "Failed at oes_api_event_recv oes_status [%d]\n",
                    oes_status);
                return;
            }

            return;
        }
        else if (FD_ISSET(fd, &input)) {
            event_recv_vs_ext = vs_ext;
            /* clear the event info */
            memset(event_info, 0x0, sizeof(event_info));

            oes_status = oes_api_event_recv(fd, &event_info[0],
                                            &event_recv_vs_ext);
            if (oes_status != OES_STATUS_SUCCESS) {
                LOG(CL_LOG_ERR,
                    "Failed at oes_api_event_recv oes_status [%d]\n",
                    oes_status);
                return;
            }

            if (muliple_fdb_notif_enabled == 1) {
                /* get number of received event */
                event_num = (int)event_recv_vs_ext;
            }
            else {
                event_num = 1;
            }

            err = ctrl_learn_handle_oes_event(event_info, event_num);
            if (err != 0) {
                if ( (err == -EXFULL) || (err == -ENOENT) ) {
                    LOG(CL_LOG_NOTICE,
                        "ctrl_learn_handle_oes_event err [%d]-[%s]\n", err, strerror(
                            -err));
                }
                else {
                    LOG(CL_LOG_ERR,
                        "ctrl_learn_handle_oes_event err [%d]-[%s]\n", err, strerror(
                            -err));
                }
            }
        }
        else if (FD_ISSET(simulate_oes_event_fd[0], &input)) {   /*else if (FD_ISSET(fd, &input)) {*/
            read(simulate_oes_event_fd[0], &bytes, sizeof(bytes));

            err = ctrl_learn_handle_oes_event(event_info_sim, event_num_sim);
            if (err == -EXFULL) {
                LOG(CL_LOG_DEBUG,
                    "ctrl_learn_handle_oes_event err [%d]-[%s]\n", err, strerror(
                        -err));
            }
            else {
                LOG(CL_LOG_ERR,
                    "ctrl_learn_handle_oes_event err [%d]-[%s]\n", err, strerror(
                        -err));
            }
        } /*else if (FD_ISSET(fd, &input)) {*/
    } /* while */
}


/* handle oes event */
int
ctrl_learn_handle_oes_event( struct oes_event_info* event_info, int event_num)
{
    int err = 0;
    int mac_addr_set_err = 0;
    struct oes_fdb_uc_mac_addr_params mac_entry_list;
    int lst_idx = 0;
    int i = 0;
    struct ctrl_learn_fdb_notify_data notif_records;
    enum oes_access_cmd access_cmd = OES_ACCESS_CMD_ADD;
    int is_learned_or_aged_event = 0;
    oes_status_e oes_status = OES_STATUS_SUCCESS;
    fdb_uc_mac_entry_t *mac_record_p = NULL;
    struct fdb_uc_key_filter key_filter;
    uint64_t db_key;
    unsigned short approved_cnt = 0;
    struct fdb_uc_mac_addr_params macs_failed_list[MAX_EVENT_INFO_SIZE];
    int macs_failed_list_len = 0;

    struct fdb_uc_mac_addr_params approved_mac_entry_list[MAX_EVENT_INFO_SIZE];

    int is_failed_mac = 0;
    int j = 0;

    /* Call Notification CallBack */

    memset(&mac_entry_list, 0x0,
           sizeof(mac_entry_list));

    lst_idx = 0;

    for (i = 0; i < event_num; i++) {
        switch (event_info[i].event_info.fdb_event.fbd_event_type) {
        case OES_FDB_EVENT_LEARN:
            /* If Approved Config the SDK Back */

            mac_entry_list.vid =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                vid;
            mac_entry_list.mac_addr =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                mac_addr;
            mac_entry_list.log_port =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                log_port;
            mac_entry_list.entry_type =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                entry_type;

            notif_records.records_arr[lst_idx].event_type =
                OES_FDB_EVENT_LEARN;
            notif_records.records_arr[lst_idx].oes_event_fdb.fbd_event_type =
                notif_records.records_arr[lst_idx].event_type;
            notif_records.records_arr[lst_idx].oes_event_fdb.fdb_event_data.
            fdb_entry.fdb_entry = mac_entry_list;
            notif_records.records_arr[lst_idx].entry_type = FDB_UC_AGEABLE;

            lst_idx++;
            access_cmd = OES_ACCESS_CMD_ADD;

            is_learned_or_aged_event = 1;
            break;
        case OES_FDB_EVENT_AGE:

            mac_entry_list.vid =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                vid;
            mac_entry_list.mac_addr =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                mac_addr;
            mac_entry_list.log_port =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                log_port;
            mac_entry_list.entry_type =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                entry_type;

            notif_records.records_arr[lst_idx].event_type = OES_FDB_EVENT_AGE;
            notif_records.records_arr[lst_idx].oes_event_fdb.fbd_event_type =
                notif_records.records_arr[lst_idx].event_type;
            notif_records.records_arr[lst_idx].oes_event_fdb.fdb_event_data.
            fdb_entry.fdb_entry = mac_entry_list;
            notif_records.records_arr[lst_idx].entry_type = FDB_UC_AGEABLE;

            lst_idx++;

            access_cmd = OES_ACCESS_CMD_DELETE;
            is_learned_or_aged_event = 1;

            break;
        case OES_FDB_EVENT_FLUSH_ALL:
            LOG(CL_LOG_DEBUG, "got event OES_FDB_EVENT_FLUSH_ALL\n");

            err = ctrl_learn_handle_flush_all();
            if (err != 0) {
                LOG(CL_LOG_ERR,
                    "Failed at handle_flush_all err [%d]\n", err);
            }
            break;
        case OES_FDB_EVENT_FLUSH_VID:


            key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
            key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_VALID;
            key_filter.vid =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                vid;
            key_filter.log_port = 0;

            LOG(CL_LOG_DEBUG,
                "got event OES_FDB_EVENT_FLUSH_VID vid [%u]\n",
                key_filter.vid);

            err = ctrl_learn_flush_db_by_filter(&key_filter);
            if (err != 0) {
                LOG(CL_LOG_ERR,
                    "Failed at ctrl_learn_flush_db_by_filter err [%d]\n", err);
                break;
            }

            break;
        case OES_FDB_EVENT_FLUSH_PORT:
            key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_VALID;
            key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;
            key_filter.log_port =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                log_port;
            key_filter.vid = 0;

            LOG(CL_LOG_DEBUG,
                "got event OES_FDB_EVENT_FLUSH_PORT log_port [%lu]\n",
                key_filter.log_port);

            err = ctrl_learn_flush_db_by_filter(&key_filter);
            if (err != 0) {
                LOG(CL_LOG_ERR,
                    "Failed at ctrl_learn_flush_db_by_filter err [%d]\n", err);
                break;
            }
            break;
        case OES_FDB_EVENT_FLUSH_PORT_VID:
            key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_VALID;
            key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_VALID;
            key_filter.vid =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                vid;
            key_filter.log_port =
                event_info[i].event_info.fdb_event.fdb_event_data.fdb_entry.
                fdb_entry.
                log_port;

            LOG(CL_LOG_DEBUG,
                "got event OES_FDB_EVENT_FLUSH_PORT_VID vid [%u] log_port [%lu]\n",
                key_filter.vid, key_filter.log_port);

            err = ctrl_learn_flush_db_by_filter(&key_filter);
            if (err != 0) {
                LOG(CL_LOG_ERR,
                    "Failed at ctrl_learn_flush_db_by_filter err [%d]\n", err);
                break;
            }
            break;
        default:
            break;
        }
    } /*for (i = 0; i < event_num ; i++)*/

    approved_cnt = 0;
    memset(approved_mac_entry_list, 0x0, sizeof(approved_mac_entry_list));

    notif_records.records_num = lst_idx;
    for (i = 0; i < (int)notif_records.records_num; i++) {
        notif_records.records_arr[i].decision =
                   CTRL_LEARN_NOTIFY_DECISION_APPROVE;
    }

    err = update_approved_list(&notif_records);
	if(err) {
		 LOG(CL_LOG_ERR,
			  "Failed at ctrl_learn build aproved list [%d]\n", err);
		 err = -EPERM;
	}

    /* is notification callback registered ? */
    if (ctrl_learn_notification_cb != NULL) {
        notif_records.records_num = lst_idx;

        err = ctrl_learn_notification_cb(&notif_records, NULL);
        if (err != 0) {
            /* Logg err */
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_notification_cb err [%d]\n",
                err);
        }
    }
    /* Iterate decision */
    approved_cnt = 0;
    for (i = 0; i < (int)notif_records.records_num; i++) {
    	if (notif_records.records_arr[i].decision ==
    			CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
    		approved_mac_entry_list[approved_cnt].mac_addr_params.vid =
    				notif_records.records_arr[i].oes_event_fdb.fdb_event_data.
    				fdb_entry.fdb_entry.vid;
    		approved_mac_entry_list[approved_cnt].mac_addr_params.mac_addr
    		=
    				notif_records.records_arr[i].oes_event_fdb.
    				fdb_event_data.
    				fdb_entry.fdb_entry.mac_addr;
    		approved_mac_entry_list[approved_cnt].mac_addr_params.log_port
    		=
    				notif_records.records_arr[i].oes_event_fdb.
    				fdb_event_data.
    				fdb_entry.fdb_entry.log_port;
    		approved_mac_entry_list[approved_cnt].mac_addr_params.
    		entry_type =
    				notif_records.records_arr[i].oes_event_fdb.fdb_event_data.
    				fdb_entry.fdb_entry.entry_type;
    		/* convert from OES Type to FDB Type */

    		switch (approved_mac_entry_list[approved_cnt].mac_addr_params.
    				entry_type) {
    		case OES_FDB_STATIC:
    			approved_mac_entry_list[approved_cnt].entry_type =
    					FDB_UC_STATIC;
    			break;
    		case OES_FDB_DYNAMIC:
    			approved_mac_entry_list[approved_cnt].entry_type =
    					FDB_UC_AGEABLE;
    			break;
    		}
    		approved_cnt++;
    	}
    }

    if (is_learned_or_aged_event) {
        if (approved_cnt != 0) {
            macs_failed_list_len = 0;
            err = ctrl_learn_hal_fdb_uc_mac_addr_set(access_cmd,
                                                     ctrl_learn_br_id,
                                                     approved_mac_entry_list,
                                                     &approved_cnt);
            if (err != 0) {
                /* ERROR */
                if (err == -EXFULL) {
                    LOG(CL_LOG_DEBUG,
                        "ctrl_learn_hal_fdb_uc_mac_addr_set err [%d]-[%s] cnt [%d]\n",
                        oes_status, strerror(-err), approved_cnt);
                }
                else if (err == -ENOENT) {
                    LOG(CL_LOG_WARN,
                        "ctrl_learn_hal_fdb_uc_mac_addr_set err [%d]-[%s] cnt [%d]\n",
                        oes_status, strerror(-err), approved_cnt);
                }
                else {
                    LOG(CL_LOG_ERR,
                        "ctrl_learn_hal_fdb_uc_mac_addr_set err [%d]-[%s]\n",
                        oes_status, strerror(-err));
                    /* keep the error to return it latter */
                    mac_addr_set_err = err;
                }
                macs_failed_list_len = approved_cnt;

                memset(macs_failed_list, 0x0, sizeof(macs_failed_list));
                /* Save list */
                for (i = 0; i < macs_failed_list_len; i++) {
                    macs_failed_list[i] = approved_mac_entry_list[i];
/*                    LOG(CL_LOG_ERR,"Failed MAC Detected [%u] fid: %4u ; mac: %x:%x:%x:%x:%x:%x ; log_port: (%lu)\n", i,
                            macs_failed_list[i].mac_addr_params.vid,
                            macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[0],
                            macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[1],
                            macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[2],
                            macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[3],
                            macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[4],
                            macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[5],
                            macs_failed_list[i].mac_addr_params.log_port);*/
                }


            }
        }

        for (i = 0; i < approved_cnt; i++) {
            if (macs_failed_list_len != 0) {
                is_failed_mac = 0;
                for (j = 0; j < macs_failed_list_len; j++) {
                    /* vid + Mac */

                    if (0 == memcmp(&approved_mac_entry_list[i].mac_addr_params
                                    , &macs_failed_list[j].mac_addr_params,
                                    sizeof(approved_mac_entry_list[i].
                                           mac_addr_params))) {
                        /*                       LOG(CL_LOG_ERR,"skip failed MAC [%u] fid: %4u ; mac: %x:%x:%x:%x:%x:%x ; log_port: (%lu)\n", i,
                                                       macs_failed_list[i].mac_addr_params.vid,
                                                       macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[0],
                                                       macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[1],
                                                       macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[2],
                                                       macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[3],
                                                       macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[4],
                                                       macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[5],
                                                       macs_failed_list[i].mac_addr_params.log_port);*/
                        is_failed_mac = 1;
                        break;
                    }
                }
                /* is the entry is a failed mac ? */
                if (is_failed_mac == 1) {
                    /* do not update the DB with failed MAC */
                    continue;
                }
            }

            if (OES_ACCESS_CMD_ADD == access_cmd) {
                /* Update FDB */
                fdb_uc_mac_entry_t mac_db_entry;

                /* Update FDB */
                memset( &mac_db_entry, 0x0, sizeof(mac_db_entry));

                mac_db_entry.mac_params =
                    approved_mac_entry_list[i].mac_addr_params;

                if (ctrl_learn_init_deinit_cb != NULL) {
                    err = ctrl_learn_init_deinit_cb(COOKIE_OP_INIT,
                                                    &mac_db_entry.cookie);
                    if (err != 0) {
                        LOG(CL_LOG_ERR,
                            "Failed at ctrl_learn_init_deinit_cb err [%d]\n",
                            err);
                    }
                }

                if (approved_mac_entry_list[i].mac_addr_params.entry_type ==
                    OES_FDB_STATIC) {
                    mac_db_entry.type = FDB_UC_STATIC;
                }
                else {
                    mac_db_entry.type = FDB_UC_AGEABLE;
                }

                cl_spinlock_acquire(&ctrl_learn_fdb_lock);
                err = fdb_uc_db_add_record(&mac_db_entry);
                if (err != 0) {
                    LOG(CL_LOG_ERR,
                        "Failed at fdb_uc_db_add_record err [%d] i [%d]\n",
                        err,
                        i);
                }
                cl_spinlock_release(&ctrl_learn_fdb_lock);
            }
            else if (OES_ACCESS_CMD_DELETE == access_cmd) {
                /* Get Record */
                db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
                    approved_mac_entry_list[i].mac_addr_params.mac_addr,
                    approved_mac_entry_list[i].mac_addr_params.vid);
                cl_spinlock_acquire(&ctrl_learn_fdb_lock);
                err = fdb_uc_db_get_record_by_key( db_key, &mac_record_p);
                if ((err != 0)) {
                    if (err != -ENOENT) {
                        LOG(CL_LOG_ERR,
                            "Failed at fdb_uc_db_get_record_by_key err [%d]-[%s] i [%d]\n", err,
                            strerror(-err),
                            i);
                    }
                }

                if (mac_record_p != NULL) {
                    if (ctrl_learn_init_deinit_cb != NULL) {
                        err = ctrl_learn_init_deinit_cb(COOKIE_OP_DEINIT,
                                                        &mac_record_p->cookie);
                        if (err != 0) {
                            LOG(CL_LOG_ERR,
                                "Failed at ctrl_learn_init_deinit_cb err [%d]\n",
                                err);
                        }
                    }


                    err = fdb_uc_db_delete_record(mac_record_p);
                    if (err) {
                        LOG(CL_LOG_ERR,
                            "Failed at fdb_uc_db_delete_record err [%d]\n",
                            err);
                    }
                }
                cl_spinlock_release(&ctrl_learn_fdb_lock);
            }    /*else if (OES_ACCESS_CMD_DELETE == access_cmd*/
        } /*    for(i = 0; i < approved_cnt; i++){*/
    } /*if (is_learned_or_aged_event){*/


    if (mac_addr_set_err != 0) {
        err = mac_addr_set_err;
    }
    return err;
}


/**
 *  This function flush the fdb according to specified key filter
 *
 *  @param[in] key_filter - pointer to key filter structure (vid/log_port/vid+log_port)
 *
 *  @return 0 when successful.
 *  @return -EPERM general error
 */
int
ctrl_learn_flush_db_by_filter(struct fdb_uc_key_filter* key_filter)
{
    fdb_uc_mac_entry_t *mac_record_p = NULL;
    fdb_uc_mac_entry_t *tmp_record = NULL;
    void *list_cookie = NULL;
    int err = 0;

    LOG(CL_LOG_DEBUG,
        "ctrl_learn_flush_db_by_filter filter by vid [%s] vid [%d] filter by log port [%s] log_port [%lu]\n",
        (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID) ? "Valid" : "Not Valid",
        key_filter->vid,
        (key_filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) ? "Valid" : "Not Valid",
        key_filter->log_port);

    cl_spinlock_acquire(&ctrl_learn_fdb_lock);
    /* Delete all DB*/
    err = fdb_uc_db_get_first_record_by_filter( key_filter, &list_cookie,
                                                &mac_record_p);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed at fdb_uc_db_get_first_record err [%d]\n", err);
        cl_spinlock_release(&ctrl_learn_fdb_lock);
        return err;
    }

    while (mac_record_p != NULL) {
        err = fdb_uc_db_get_next_record_by_filter( key_filter, list_cookie,
                                                   mac_record_p, &tmp_record);
        if (err != 0) {
            LOG(CL_LOG_ERR,
                "Failed at fdb_uc_db_get_next_record_by_filter err [%d]\n",
                err);
        }


        if (mac_record_p->type != FDB_UC_STATIC) {
            if (ctrl_learn_init_deinit_cb != NULL) {
                err = ctrl_learn_init_deinit_cb(COOKIE_OP_DEINIT,
                                                &mac_record_p->cookie);
                if (err != 0) {
                    LOG(CL_LOG_ERR,
                        "Failed at ctrl_learn_init_deinit_cb err [%d]\n", err);
                }
            }

            err = fdb_uc_db_delete_record(mac_record_p);
            if (err) {
                LOG(CL_LOG_ERR,
                    "Failed at fdb_uc_db_delete_record err [%d]\n", err);
            }
        }

        mac_record_p = tmp_record;
    }
    cl_spinlock_release(&ctrl_learn_fdb_lock);

    return err;
}

/**
 *  This functions start the Control learning library. It start listening to FDB event
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int
ctrl_learn_start(void)
{
    /* Send start to thread */
    cl_event_signal(&(wait_for_start_event));

    return 0;
}

/**
 *  Stop Control learning functionality
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int
ctrl_learn_stop(void)
{
    /* Stop Thread */
    int bytes = 0;
    int err = 0;

    if (is_ctrl_learn_initialized == 0) {
        goto out;
    }

    /* signal stop */
    ctrl_learn_stop_signal = 1;

    if (is_ctrl_learn_start == 0) {
        /* send start event to thread to release it*/
        cl_event_signal(&(wait_for_start_event));
        goto out;
    }

    /* Terminate control learn thread */
    bytes = write(quit_ctrl_learn_thread_fd[1], &bytes, sizeof(bytes));
    if (bytes != sizeof(bytes)) {
        LOG(CL_LOG_ERR, "Ctrl Learn thread deinit failed.\n");
        err = -EPERM;
        goto out;
    }

    is_ctrl_learn_start = 0;
out:
    return err;
}

/**
 *  This function adds UC MAC and UC LAG MAC entries in the FDB.
 *  currently it *
 * @param[in] access_cmd - add/ delete
 * @param[in] mac_entry_p - mac record arry pointer . On
 *       deletion, entry_type is DONT_CARE
 * @param[in] mac_cnt - mac record array size
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 - Operation completes successfully,
 * @return -EINVAL - Unsupported verbosity_level
 * @return -EPERM general error.
 */
int
ctrl_learn_simulate_oes_event(
    enum oes_access_cmd access_cmd,
    const struct fdb_uc_mac_addr_params *mac_entry,
    unsigned short mac_cnt)
{
    int err = 0;
    int i = 0;

    /* Validity check - verify not null */
    if (NULL == mac_entry) {
        LOG(CL_LOG_ERR, "FDB: mac_entry_list is null\n");
        err = -EINVAL;
        goto out;
    }

    /* TODO update oes to change to pointer */
    if (0 /*NULL*/ == mac_cnt) {
        LOG(CL_LOG_ERR, "FDB: data_cnt is null\n");
        err = -EINVAL;
        goto out;
    }

    if ((OES_ACCESS_CMD_ADD != access_cmd) &&
        (OES_ACCESS_CMD_DELETE != access_cmd)) {
        LOG(CL_LOG_ERR, "access command unsupported\n");
        err = -EINVAL;
        goto out;
    }

    /* Validity check - verify data_cnt > 0 */
    if (0 == mac_cnt) {
        LOG(CL_LOG_ERR, "FDB: mac_entry_num is zero\n");
        err = -EINVAL;
        goto out;
    }

    /* Validity check - verify the MAC is UC*/
    for (i = 0; i < mac_cnt; i++) {
        if (mac_entry[i].mac_addr_params.mac_addr.ether_addr_octet[0] &
            FDB_GROUP_ADDRESS_MASK) {
            LOG(CL_LOG_ERR,
                "FDB: At least one MAC address is not a UC address\n");
            err = -EINVAL;
            goto out;
        }

        if (access_cmd != OES_ACCESS_CMD_DELETE) {
            if (!(mac_entry[i].entry_type <= FDB_ENTRY_TYPE_MAX)) {
                LOG(CL_LOG_ERR,
                    "FDB: MAC entry type exceeds range (%u-%u).\n",
                    FDB_ENTRY_TYPE_MIN, FDB_ENTRY_TYPE_MAX);
                err = -EINVAL;
                goto out;
            }
        }
    }

    memset(event_info_sim, 0x0, MAX_EVENT_INFO_SIZE);
    event_num_sim = 0;

    for (i = 0; i < mac_cnt; i++) { /*for(i = 0; i < approved_cnt; i++){*/
        if (OES_ACCESS_CMD_ADD == access_cmd) {
            /* prepare OES event */

            event_info_sim[i].event_id = OES_EVENT_ID_FDB;

            event_info_sim[i].event_info.fdb_event.fbd_event_type =
                OES_FDB_EVENT_LEARN;

            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            vid = mac_entry[i].mac_addr_params.vid;
            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            mac_addr = mac_entry[i].mac_addr_params.mac_addr;

            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            log_port = mac_entry[i].mac_addr_params.log_port;

            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            entry_type = mac_entry[i].mac_addr_params.entry_type;
        }
        else if (OES_ACCESS_CMD_DELETE == access_cmd) {
            event_info_sim[i].event_id = OES_EVENT_ID_FDB;

            event_info_sim[i].event_info.fdb_event.fbd_event_type =
                OES_FDB_EVENT_AGE;

            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            vid = mac_entry[i].mac_addr_params.vid;
            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            mac_addr = mac_entry[i].mac_addr_params.mac_addr;

            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            log_port = mac_entry[i].mac_addr_params.log_port;

            event_info_sim[i].event_info.fdb_event.fdb_event_data.fdb_entry.
            fdb_entry.
            entry_type = mac_entry[i].mac_addr_params.entry_type;
        }    /*else if (OES_ACCESS_CMD_DELETE == access_cmd*/

        event_num_sim++;
    } /*for (i = 0; i < (int)notif_records.records_num; i++){*/

    int bytes = 0;


    /* Terminate control learn thread */
    bytes = write(simulate_oes_event_fd[1], &bytes, sizeof(bytes));
    if (bytes != sizeof(bytes)) {
        LOG(CL_LOG_ERR, "Ctrl Learn simulate oes event failed.\n");
        err = -EPERM;
        goto out;
    }

out:

    return err;
}



/**
 *  This function de-inits the Control learning library - cleanup memory,structures,etc..
 *
 *  @return 0 when successful.
 *  @return -EPERM general error
 */
int
ctrl_learn_deinit(void)
{
    int err = 0;

    if (is_ctrl_learn_initialized == 0) {
        goto out;
    }

    if (ctrl_learn_stop_signal == 0) {
        err = ctrl_learn_stop();
        if (err != 0) {
            LOG(CL_LOG_ERR,  "ctrl_learn_deinit: Failed at ctrl_learn_stop\n");
            return err;
        }
    }

    cl_thread_destroy(&ctrl_learn_thread);

    /* destroy cl event */
    cl_event_destroy(&(wait_for_start_event));

    close(quit_ctrl_learn_thread_fd[0]);
    close(quit_ctrl_learn_thread_fd[1]);

    close(simulate_oes_event_fd[0]);
    close(simulate_oes_event_fd[1]);

    cl_spinlock_destroy(&ctrl_learn_fdb_lock);

    /* fdb uc db deinit */
    err = fdb_uc_db_deinit();
    if (err != 0) {
        err = -EPERM;
        return err;
    }

out:
    return 0;
}




/**
 *  This function adds UC MAC and UC LAG MAC entries in the FDB.
 *  currently it *
 * @param[in] access_cmd - add/ delete
 * @param[in,out] mac_entry_p - mac record arry pointer . On
 *       deletion, entry_type is DONT_CARE
 * @param[in,out] mac_cnt - mac record array size
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 * @param[in] fdb_lock - lock/unlock the FDB while accessing it.  1 - lock , 0 - don't lock
 *
 * @return 0 - Operation completes successfully,
 * @return -EINVAL - Unsupported verbosity_level
 * @return -EPERM general error.
 */
int
ctrl_learn_api_fdb_uc_mac_addr_set(
    enum oes_access_cmd access_cmd,
    struct fdb_uc_mac_addr_params *mac_entry,
    unsigned short  * mac_cnt,
    const void *originator_cookie,
    const int fdb_lock)
{
    fdb_uc_mac_entry_t *mac_record_p = NULL;
    uint64_t db_key;
    int err = 0;
    int mac_addr_set_err = 0;
    int i = 0;
    int j = 0;
    int *is_exist_status = NULL;
    int lst_idx = 0;
    struct fdb_uc_mac_addr_params *mac_entry_list = NULL;
    struct fdb_uc_mac_addr_params *approved_mac_entry_list = NULL;
    struct fdb_uc_mac_addr_params *macs_failed_list = NULL;
    int macs_failed_list_len = 0;
    unsigned short approved_cnt = 0;
    struct ctrl_learn_fdb_notify_data notify_records;
    void *mac_entry_cookie = NULL;
    int num_macs = 0;
    int is_failed_mac = 0;

    if (is_ctrl_learn_start == 0) {
        LOG(CL_LOG_WARN,
            "Warning cannot perform uc mac addr set since Ctrl learn is not started yet\n");
        goto out;
    }

    /* Validity check - verify not null */
    if (mac_entry == NULL) {
        LOG(CL_LOG_ERR, "FDB: mac_entry_list is null\n");
        err = -EINVAL;
        goto out;
    }

    if (mac_cnt == NULL) {
        LOG(CL_LOG_ERR, "FDB: data_cnt is null\n");
        err = -EINVAL;
        goto out;
    }

    num_macs = *mac_cnt;
    *mac_cnt = 0; /* denied macs*/
    /* Validity check - verify data_cnt > 0 */
    if (num_macs == 0) {
        LOG(CL_LOG_ERR, "FDB: mac_entry_num is zero\n");
        err = -EINVAL;
        goto out;
    }

    if ((access_cmd != OES_ACCESS_CMD_ADD) &&
        (access_cmd != OES_ACCESS_CMD_DELETE)) {
        LOG(CL_LOG_ERR, "access command unsupported\n");
        err = -EINVAL;
        goto out;
    }

    /* Validity check - verify the MAC is UC*/
    for (i = 0; i < num_macs; i++) {
        if (mac_entry[i].mac_addr_params.mac_addr.ether_addr_octet[0] &
            FDB_GROUP_ADDRESS_MASK) {
            LOG(CL_LOG_ERR,
                "FDB: At least one MAC address is not a UC address\n");
            err = -EINVAL;
            goto out;
        }

        if (access_cmd != OES_ACCESS_CMD_DELETE) {
            if (mac_entry[i].entry_type > FDB_ENTRY_TYPE_MAX) {
                LOG(CL_LOG_ERR,
                    "FDB: MAC entry type exceeds range (%u-%u).\n",
                    FDB_ENTRY_TYPE_MIN, FDB_ENTRY_TYPE_MAX);
                err = -EINVAL;
                goto out;
            }
        }
    }

    mac_entry_list =
        (struct fdb_uc_mac_addr_params*)malloc(
            sizeof(struct fdb_uc_mac_addr_params) * num_macs);

    if (mac_entry_list == NULL) {
        LOG(CL_LOG_ERR,
            "Failed to allocate memory for mac_entry_list\n");
        err = -ENOMEM;
        goto out;
    }


    is_exist_status = (int*)malloc(sizeof(int) * num_macs);

    if (is_exist_status == NULL) {
        LOG(CL_LOG_ERR,
            "Failed to allocate memory for is_exist_status\n");
        err = -ENOMEM;
        goto out;
    }
    for (i = 0; i < num_macs; i++) {
        is_exist_status[i] = 0;
        /* Check if FDB Entry Exist */

        /* Get Record */
        db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
            mac_entry[i].mac_addr_params.mac_addr,
            mac_entry[i].mac_addr_params.vid);
        err = fdb_uc_db_get_record_by_key( db_key, &mac_record_p);
        if (err != 0) {
            if (err == -ENOENT) {
                is_exist_status[i] = 0;
                err = 0;
            }
            else {
                SX_LOG_ERR("FDB: failed to check if same entry exists (%s)\n",
                           strerror(-err));
                continue;
            }
        }

        if (mac_record_p != NULL) {
            is_exist_status[i] = 1;
        }

        if (access_cmd == OES_ACCESS_CMD_ADD) {
            if (is_exist_status[i] == 1) {
                /* already in fdb. check if exactly the same or not */
                if ((mac_record_p->mac_params.entry_type ==
                     mac_entry[i].mac_addr_params.entry_type) &&
                    (mac_record_p->mac_params.log_port ==
                     mac_entry[i].mac_addr_params.log_port) &&
                    (mac_record_p->type == mac_entry[i].entry_type)) {
                    /* trying to add an entry that is already exist */
                    continue;
                }

                mac_entry_cookie = mac_record_p->cookie;
            }
            notify_records.records_arr[lst_idx].event_type =
                OES_FDB_EVENT_LEARN;
        }
        else if (access_cmd == OES_ACCESS_CMD_DELETE) {
            if (is_exist_status[i] == 0) {
                /* trying to delete an entry that does not exist */
                /* nothing to do */
                continue;
            }
            notify_records.records_arr[lst_idx].event_type = OES_FDB_EVENT_AGE;
        }

        mac_entry_list[lst_idx].mac_addr_params = mac_entry[i].mac_addr_params;

        notify_records.records_arr[lst_idx].oes_event_fdb.fbd_event_type =
            notify_records.records_arr[lst_idx].event_type;
        notify_records.records_arr[lst_idx].oes_event_fdb.fdb_event_data.
        fdb_entry.fdb_entry = mac_entry[i].mac_addr_params;

        notify_records.records_arr[lst_idx].entry_type =
            mac_entry[i].entry_type;
        notify_records.records_arr[lst_idx].cookie = mac_entry_cookie;
        if (ctrl_learn_notification_cb == NULL) {
            notify_records.records_arr[lst_idx].decision =
                CTRL_LEARN_NOTIFY_DECISION_APPROVE;
        }
        lst_idx++;
    }

    notify_records.records_num = lst_idx;
    for (i = 0; i < (int)notify_records.records_num; i++) {
            notify_records.records_arr[i].decision =
                       CTRL_LEARN_NOTIFY_DECISION_APPROVE;
    }

    err = update_approved_list(&notify_records);
	if(err) {
         LOG(CL_LOG_ERR,
           "Failed at ctrl_learn build aproved list [%d]\n", err);
         err = -EPERM;
         goto out;
    }
    /* is notification callback registered ? */
    if (ctrl_learn_notification_cb != NULL) {
        err = ctrl_learn_notification_cb(&notify_records, originator_cookie);
        if (err != 0) {
            /* Log err */
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_notification_cb err [%d]\n", err);
            err = -ENOMEM;
            goto out;
        }
    }

    approved_mac_entry_list = (struct fdb_uc_mac_addr_params*)malloc(
    		sizeof(struct fdb_uc_mac_addr_params) * num_macs);

    if (approved_mac_entry_list == NULL) {
    	LOG(CL_LOG_ERR,
    			"Failed to allocate memory for approved_mac_entry_list\n");
    	err = -ENOMEM;
    	goto out;
    }

    /* Iterate decision */
    approved_cnt = 0;
    for (i = 0; i < (int)notify_records.records_num; i++) {
    	if (notify_records.records_arr[i].decision ==
    			CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
    		approved_mac_entry_list[approved_cnt].mac_addr_params.vid =
    				notify_records.records_arr[i].oes_event_fdb.fdb_event_data.
    				fdb_entry.fdb_entry.vid;
    		approved_mac_entry_list[approved_cnt].mac_addr_params.mac_addr
    		=
    				notify_records.records_arr[i].oes_event_fdb.
    				fdb_event_data.
    				fdb_entry.fdb_entry.mac_addr;
    		approved_mac_entry_list[approved_cnt].mac_addr_params.log_port
    		=
    				notify_records.records_arr[i].oes_event_fdb.
    				fdb_event_data.
    				fdb_entry.fdb_entry.log_port;

    		approved_mac_entry_list[approved_cnt].entry_type =
    				notify_records.records_arr[i].entry_type;

    		approved_cnt++;
    	}
    }

    /* check if approved mac list is not empty */
    if (approved_cnt != 0) {
        err = ctrl_learn_hal_fdb_uc_mac_addr_set(access_cmd, ctrl_learn_br_id,
                                                 approved_mac_entry_list,
                                                 &approved_cnt);
        if (err != 0) {
            if (err == -EXFULL) {
                LOG(CL_LOG_DEBUG,
                    "ctrl_learn_hal_fdb_uc_mac_addr_set failed err [%d]-[%s] cnt [%d]\n",
                    err, strerror(-err), approved_cnt);
            }
            else if (err == -ENOENT) {
                LOG(CL_LOG_WARN,
                    "ctrl_learn_hal_fdb_uc_mac_addr_set failed err [%d]-[%s] cnt [%d]\n",
                    err, strerror(-err), approved_cnt);
            }
            else {
                /* ERROR */
                LOG(CL_LOG_ERR,
                    "ctrl_learn_hal_fdb_uc_mac_addr_set failed err [%d]-[%s]\n",
                    err, strerror(-err));
            }

            /* keep the error to return it latter */
            mac_addr_set_err = err;
            macs_failed_list_len = approved_cnt;

            macs_failed_list = (struct fdb_uc_mac_addr_params*)malloc(
                sizeof(struct fdb_uc_mac_addr_params) * macs_failed_list_len);

            if (macs_failed_list == NULL) {
                LOG(CL_LOG_ERR,
                    "Failed to allocate memory for macs_failed_list\n");
                err = -ENOMEM;
                goto out;
            }


            memset(macs_failed_list, 0x0,
                   sizeof(struct fdb_uc_mac_addr_params) *
                   macs_failed_list_len);
            /* Save list */
            for (i = 0; i < macs_failed_list_len; i++) {
                macs_failed_list[i] = approved_mac_entry_list[i];
                mac_entry[i] = approved_mac_entry_list[i];

/*                LOG(CL_LOG_ERR,"Failed MAC Detected [%u] fid: %4u ; mac: %x:%x:%x:%x:%x:%x ; log_port: (%lu)\n", i,
                        macs_failed_list[i].mac_addr_params.vid,
                        macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[0],
                        macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[1],
                        macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[2],
                        macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[3],
                        macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[4],
                        macs_failed_list[i].mac_addr_params.mac_addr.ether_addr_octet[5],
                        macs_failed_list[i].mac_addr_params.log_port); */
            }

            *mac_cnt = macs_failed_list_len;
        }
    }

    for (i = 0; i < (int)notify_records.records_num; i++) {
        if (notify_records.records_arr[i].decision ==
            CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
            if (macs_failed_list != NULL) {
                is_failed_mac = 0;
                for (j = 0; j < macs_failed_list_len; j++) {
                    /* vid + Mac */

                    if ((notify_records.records_arr[i].oes_event_fdb.
                         fdb_event_data.fdb_entry.fdb_entry.vid ==
                         macs_failed_list[j].mac_addr_params.vid)

                        && (notify_records.records_arr[i].oes_event_fdb.
                            fdb_event_data.fdb_entry.fdb_entry.log_port ==
                            macs_failed_list[j].mac_addr_params.log_port)

                        && (notify_records.records_arr[i].entry_type ==
                            macs_failed_list[j].entry_type)

                        && (0 ==
                            memcmp(&notify_records.records_arr[i].oes_event_fdb
                                   .
                                   fdb_event_data.fdb_entry.fdb_entry.mac_addr,
                                   &macs_failed_list[j].mac_addr_params.
                                   mac_addr,
                                   sizeof(macs_failed_list[j].mac_addr_params.
                                          mac_addr)))) {
                        /*LOG(CL_LOG_ERR,"skip failed MAC [%u] fid: %4u ; mac: %x:%x:%x:%x:%x:%x ; log_port: (%lu)\n", j,
                                    macs_failed_list[j].mac_addr_params.vid,
                                    macs_failed_list[j].mac_addr_params.mac_addr.ether_addr_octet[0],
                                    macs_failed_list[j].mac_addr_params.mac_addr.ether_addr_octet[1],
                                    macs_failed_list[j].mac_addr_params.mac_addr.ether_addr_octet[2],
                                    macs_failed_list[j].mac_addr_params.mac_addr.ether_addr_octet[3],
                                    macs_failed_list[j].mac_addr_params.mac_addr.ether_addr_octet[4],
                                    macs_failed_list[j].mac_addr_params.mac_addr.ether_addr_octet[5],
                                    macs_failed_list[j].mac_addr_params.log_port);*/
                        is_failed_mac = 1;
                        break;
                    }
                }
                /* is the entry is a failed mac ? */
                if (is_failed_mac == 1) {
                    /* do not update the DB with failed MAC */
                    continue;
                }
            } /*if (macs_failed_list != NULL){ */
            if (access_cmd == OES_ACCESS_CMD_ADD) {
                /* Update FDB */
                fdb_uc_mac_entry_t mac_db_entry;

                /* Update FDB */
                memset( &mac_db_entry, 0x0, sizeof(mac_db_entry));

                mac_db_entry.mac_params.vid =
                    notify_records.records_arr[i].oes_event_fdb.fdb_event_data.
                    fdb_entry.fdb_entry.vid;
                mac_db_entry.mac_params.mac_addr =
                    notify_records.records_arr[i].oes_event_fdb.fdb_event_data.
                    fdb_entry.fdb_entry.mac_addr;
                mac_db_entry.mac_params.log_port =
                    notify_records.records_arr[i].oes_event_fdb.fdb_event_data.
                    fdb_entry.fdb_entry.log_port;

                if (notify_records.records_arr[i].entry_type ==
                    FDB_UC_STATIC) {
                    mac_db_entry.mac_params.entry_type = OES_FDB_STATIC;
                }
                else {
                    mac_db_entry.mac_params.entry_type = OES_FDB_DYNAMIC;
                }

                mac_db_entry.type = notify_records.records_arr[i].entry_type;

                /* incase of modify - keep the cookie */
                if (notify_records.records_arr[i].cookie != NULL) {
                    mac_db_entry.cookie = notify_records.records_arr[i].cookie;
                }
                else {
                    /* incase of new entry call init */
                    if (ctrl_learn_init_deinit_cb != NULL) {
                        err = ctrl_learn_init_deinit_cb(COOKIE_OP_INIT,
                                                        &mac_db_entry.cookie);
                        if (err != 0) {
                            LOG(CL_LOG_ERR,
                                "Failed at ctrl_learn_init_deinit_cb err [%d]\n",
                                err);
                            goto out;
                        }
                    }
                }

                if (fdb_lock == 1) {
                    cl_spinlock_acquire(&ctrl_learn_fdb_lock);
                }
                err = fdb_uc_db_add_record(&mac_db_entry);
                if (err != 0) {
                    LOG(CL_LOG_ERR,
                        "Failed at fdb_uc_db_add_record err [%d] i [%d]\n",
                        err,
                        i);
                    if (fdb_lock == 1) {
                        cl_spinlock_release(&ctrl_learn_fdb_lock);
                    }
                    goto out;
                }
                if (fdb_lock == 1) {
                    cl_spinlock_release(&ctrl_learn_fdb_lock);
                }
            }
            else if (access_cmd == OES_ACCESS_CMD_DELETE) {
                /* Get Record */
                db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
                    notify_records.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.mac_addr,
                    notify_records.records_arr[i].oes_event_fdb.fdb_event_data.fdb_entry.fdb_entry.vid);
                if (fdb_lock == 1) {
                    cl_spinlock_acquire(&ctrl_learn_fdb_lock);
                }
                err = fdb_uc_db_get_record_by_key( db_key, &mac_record_p);
                if ((err != 0)) {
                    if (err != -ENOENT) {
                        LOG(CL_LOG_ERR,
                            "Failed at fdb_uc_db_get_record_by_key err [%d]-[%s] i [%d]\n", err,
                            strerror(-err),
                            i);
                    }
                }

                if (mac_record_p != NULL) {
                    if (ctrl_learn_init_deinit_cb != NULL) {
                        err = ctrl_learn_init_deinit_cb(COOKIE_OP_DEINIT,
                                                        &mac_record_p->cookie);
                        if (err != 0) {
                            LOG(CL_LOG_ERR,
                                "Failed at ctrl_learn_init_deinit_cb err [%d]\n",
                                err);
                        }
                    }


                    err = fdb_uc_db_delete_record(mac_record_p);
                    if (err) {
                        LOG(CL_LOG_ERR,
                            "Failed at fdb_uc_db_delete_record err [%d]\n",
                            err);
                        if (fdb_lock == 1) {
                            cl_spinlock_release(&ctrl_learn_fdb_lock);
                        }
                        goto out;
                    }
                }
                if (fdb_lock == 1) {
                    cl_spinlock_release(&ctrl_learn_fdb_lock);
                }
            } /*else if (OES_ACCESS_CMD_DELETE == access_cmd*/
        } /*if (notif_records.records_arr[i].decision == CTRL_LEARN_NOTIFY_DECISION_APPROVE){*/
    } /*for (i = 0; i < (int)notif_records.records_num; i++){*/

out:
    if (mac_entry_list != NULL) {
        free(mac_entry_list);
    }

    if (is_exist_status != NULL) {
        free(is_exist_status);
    }

    if (approved_mac_entry_list != NULL) {
        free(approved_mac_entry_list);
    }

    if (macs_failed_list != NULL) {
        free(macs_failed_list);
    }

    /* check if had an error in mac addr set */
    if (mac_addr_set_err != 0) {
        err = mac_addr_set_err;
    }

    return err;
}

/**
 *  This function updates list of approved for configure MAC entries.
 *  Verified data base limitations
 *  @param[in,out] motif_records - notification records
 */
int
update_approved_list(struct ctrl_learn_fdb_notify_data* notif_records)
{
    int err = 0;
    uint32_t i,  cnt;
    uint32_t num_approved = notif_records->records_num;
    uint32_t processed_approved =0;

	err = fdb_uc_db_get_free_pool_count(&cnt);
	if(err){
        LOG(CL_LOG_ERR,
        "Failed getting counter of free blocks ,err %d\n", err);
        goto bail;
    }
    if(cnt < notif_records->records_num){
        num_approved = cnt;

       /* after passing "num_approved" elements mark as "denied"
         * all entries with LEARN TYPE  */
       for (i = 0; i < notif_records->records_num; i++) {
           if(notif_records->records_arr[i].event_type == OES_FDB_EVENT_LEARN){

               if(processed_approved >= num_approved){
                    notif_records->records_arr[i].decision =
                    CTRL_LEARN_NOTIFY_DECISION_DENY;
               }
               processed_approved ++;
          }
       }
    }
 bail:
    return err;
}




/**
 *  This function flush the fdb
 *
 *  @param[in] key_filter - pointer to key filter structure (vid/log_port/vid+log_port)
 *
 *  @return 0 when successful.
 *  @return -EPERM general error
 */
int
ctrl_learn_handle_flush_all(void)
{
    int err = 0;
    fdb_uc_mac_entry_t *mac_record_p = NULL;
    fdb_uc_mac_entry_t *tmp_record = NULL;

    cl_spinlock_acquire(&ctrl_learn_fdb_lock);

    /* Delete all DB */
    err = fdb_uc_db_get_first_record(&mac_record_p);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed at fdb_uc_db_get_first_record err [%d]\n", err);
        cl_spinlock_release(&ctrl_learn_fdb_lock);
        return err;
    }

    while (mac_record_p != NULL) {
        err = fdb_uc_db_next_record(mac_record_p, &tmp_record);
        if (err != 0) {
            LOG(CL_LOG_ERR,
                "Failed at fdb_uc_db_next_record err [%d]\n", err);
        }


        if (mac_record_p->type != FDB_UC_STATIC) {
            if (ctrl_learn_init_deinit_cb != NULL) {
                err = ctrl_learn_init_deinit_cb(COOKIE_OP_DEINIT,
                                                &mac_record_p->cookie);
                if (err != 0) {
                    LOG(CL_LOG_ERR,
                        "Failed at ctrl_learn_init_deinit_cb err [%d]\n", err);
                }
            }


            err = fdb_uc_db_delete_record(mac_record_p);
            if (err) {
                LOG(CL_LOG_ERR,
                    "Failed at fdb_uc_db_delete_record err [%d]\n", err);
            }
        }

        mac_record_p = tmp_record;
    }

    cl_spinlock_release(&ctrl_learn_fdb_lock);

    return err;
}

/**
 * This function deletes all of the FDB table.
 *
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EPRM general error.
 */
int
ctrl_learn_api_fdb_uc_flush_set(const void *originator_cookie)
{
    struct ctrl_learn_fdb_notify_data notif_records;
    int err = 0;
    oes_status_e oes_status = OES_STATUS_SUCCESS;

    if (is_ctrl_learn_start == 0) {
        LOG(CL_LOG_WARN,
            "Warning cannot perform uc flush since Ctrl learn is not started yet\n");
        goto out;
    }

    /* is notification callback registered ? */
    if (ctrl_learn_notification_cb != NULL) {
        notif_records.records_num = 1;
        notif_records.records_arr[0].event_type = OES_FDB_EVENT_FLUSH_ALL;

        err = ctrl_learn_notification_cb(&notif_records, originator_cookie);
        if (err != 0) {
            /* Log err */
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_notification_cb err [%d]\n",
                err);
        }

        if (notif_records.records_arr[0].decision !=
            CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
            err = 0;
            goto out;
        }
    }

    oes_status = oes_api_fdb_uc_flush_set(ctrl_learn_br_id,
                                          NULL);

    if (oes_status != OES_STATUS_SUCCESS) {
        /* ERROR */
        LOG(CL_LOG_ERR,
            "Failed at oes_api_fdb_uc_flush_set oes_status [%d]\n",
            oes_status);
    }
#ifdef SIMULATE_MODE
    ctrl_learn_handle_flush_all();
#endif

out:
    return err;
}

/**
 *  This function deletes the FDB table entries that are related
 *  to a flushed port.
 *
 * @param[in] log_port- logical port ID
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EINVAL - invalid argument
 * @return -EPRM general error.
 *
 */
int
ctrl_learn_api_fdb_uc_flush_port_set(unsigned long log_port,
                                     const void *originator_cookie)
{
    struct ctrl_learn_fdb_notify_data notif_records;
    int err = 0;
    oes_status_e oes_status = OES_STATUS_SUCCESS;

    if (is_ctrl_learn_start == 0) {
        LOG(CL_LOG_WARN,
            "Warning cannot perform uc flush port since Ctrl learn is not started yet log_port [%lu]\n"
            ,
            log_port);
        goto out;
    }

    /* is notification callback registered ? */
    if (ctrl_learn_notification_cb != NULL) {
        notif_records.records_num = 1;
        notif_records.records_arr[0].event_type = OES_FDB_EVENT_FLUSH_PORT;
        notif_records.records_arr[0].oes_event_fdb.fdb_event_data.fdb_port.port
            = log_port;

        err = ctrl_learn_notification_cb(&notif_records, originator_cookie);
        if (err != 0) {
            /* Logg err */
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_notification_cb err [%d]\n",
                err);
            goto out;
        }

        if (notif_records.records_arr[0].decision !=
            CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
            err = 0;
            goto out;
        }
    }

    LOG(CL_LOG_DEBUG,
        "calling oes_api_fdb_uc_flush_port_set ctrl_learn_br_id [%d] log_port [%lu]\n",
        ctrl_learn_br_id, log_port);

    oes_status = oes_api_fdb_uc_flush_port_set(ctrl_learn_br_id,
                                               log_port,
                                               NULL);

    if (oes_status != OES_STATUS_SUCCESS) {
        /* ERROR */
        LOG(CL_LOG_ERR,
            "Failed at oes_api_fdb_uc_flush_set oes_status [%d]\n",
            oes_status);
        err = oes_status_to_errno(oes_status);
        goto out;
    }

#ifdef SIMULATE_MODE
    struct fdb_uc_key_filter key_filter;
    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_NOT_VALID;
    key_filter.log_port = log_port;

    err = ctrl_learn_flush_db_by_filter(&key_filter);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed at ctrl_learn_flush_db_by_filter err [%d]\n", err);
        err = -EPERM;
    }
#endif

out:
    return err;
}

/**
 * This function deletes all FDB table entries that were
 * learned on the flushed FID
 *
 * @param[in] vid- vlan ID
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EINVAL - invalid agrgument
 * @return -EPRM general error.
 *
 */
int
ctrl_learn_api_fdb_uc_flush_vid_set(
    unsigned short vid,
    const void *originator_cookie)
{
    struct ctrl_learn_fdb_notify_data notif_records;
    int err = 0;
    oes_status_e oes_status = OES_STATUS_SUCCESS;

    if (is_ctrl_learn_start == 0) {
        LOG(CL_LOG_WARN,
            "Warning cannot perform uc flush vid since Ctrl learn is not started yet vid [%d]\n",
            vid);
        goto out;
    }

    /* is notification callback registered ? */
    if (ctrl_learn_notification_cb != NULL) {
        notif_records.records_num = 1;
        notif_records.records_arr[0].event_type = OES_FDB_EVENT_FLUSH_VID;
        notif_records.records_arr[0].oes_event_fdb.fdb_event_data.fdb_vid.vid =
            vid;

        err = ctrl_learn_notification_cb(&notif_records, originator_cookie);
        if (err != 0) {
            /* Logg err */
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_notification_cb err [%d]\n",
                err);
        }

        if (notif_records.records_arr[0].decision !=
            CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
            err = 0;
            goto out;
        }
    }

    LOG(CL_LOG_DEBUG,
        "calling oes_api_fdb_uc_flush_vid_set ctrl_learn_br_id [%d] vid [%u]\n",
        ctrl_learn_br_id, vid);

    oes_status = oes_api_fdb_uc_flush_vid_set(ctrl_learn_br_id,
                                              vid,
                                              NULL);

    if (oes_status != OES_STATUS_SUCCESS) {
        /* ERROR */
        LOG(CL_LOG_ERR,
            "Failed at oes_api_fdb_uc_flush_set oes_status [%d]\n",
            oes_status);
        err = oes_status_to_errno(oes_status);
        goto out;
    }


#ifdef SIMULATE_MODE
    struct fdb_uc_key_filter key_filter;
    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_VALID;
    key_filter.vid = vid;

    err = ctrl_learn_flush_db_by_filter(&key_filter);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed at ctrl_learn_flush_db_by_filter err [%d]\n", err);
        err = -EPERM;
    }

#endif

out:
    return err;
}


/**
 * This function deletes all FDB table entries that were
 * learned on the flushed VID and port.
 *
 * @param[in] vid- vlan ID
 * @param[in] log_port- logical port ID
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EINVAL - invalid agrgument
 * @return -EPRM general error.
 */
int
ctrl_learn_api_fdb_uc_flush_port_vid_set(
    unsigned short vid,
    unsigned long log_port,
    const void *originator_cookie)
{
    struct ctrl_learn_fdb_notify_data notif_records;
    int err = 0;
    oes_status_e oes_status = OES_STATUS_SUCCESS;

    if (is_ctrl_learn_start == 0) {
        LOG(CL_LOG_WARN,
            "Warning cannot perform uc flush port vid since Ctrl learn is not started yet vid [%d] log_port [%lu]\n"
            , vid,
            log_port);
        goto out;
    }

    /* is notification callback registered ? */
    if (ctrl_learn_notification_cb != NULL) {
        notif_records.records_num = 1;
        notif_records.records_arr[0].event_type = OES_FDB_EVENT_FLUSH_PORT_VID;
        notif_records.records_arr[0].oes_event_fdb.fdb_event_data.fdb_port_vid.
        vid = vid;
        notif_records.records_arr[0].oes_event_fdb.fdb_event_data.fdb_port_vid.
        port = log_port;


        err = ctrl_learn_notification_cb(&notif_records, originator_cookie);
        if (err != 0) {
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_notification_cb err [%d]\n",
                err);
        }

        if (notif_records.records_arr[0].decision !=
            CTRL_LEARN_NOTIFY_DECISION_APPROVE) {
            err = 0;
            goto out;
        }
    }

    LOG(CL_LOG_DEBUG,
        "calling oes_api_fdb_uc_flush_port_fid_set ctrl_learn_br_id [%d] vid [%d] log_port [%lu]\n",
        ctrl_learn_br_id, vid, log_port);

    oes_status = oes_api_fdb_uc_flush_port_vid_set(ctrl_learn_br_id,
                                                   vid,
                                                   log_port,
                                                   NULL);

    if (oes_status != OES_STATUS_SUCCESS) {
        /* ERROR */
        LOG(CL_LOG_ERR,
            "Failed at oes_api_fdb_uc_flush_set oes_status [%d]\n",
            oes_status);
        err = oes_status_to_errno(oes_status);
        goto out;
    }

#ifdef SIMULATE_MODE
    struct fdb_uc_key_filter key_filter;

    key_filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_VALID;
    key_filter.filter_by_vid = FDB_KEY_FILTER_FIELD_VALID;
    key_filter.vid = vid;
    key_filter.log_port = log_port;


    err = ctrl_learn_flush_db_by_filter(&key_filter);
    if (err != 0) {
        LOG(CL_LOG_ERR,
            "Failed at ctrl_learn_flush_db_by_filter err [%d]\n", err);
        err = -EPERM;
    }
#endif

out:
    return err;
}

/**
 * This function reads MAC entries from the SW FDB table, which
 * is exact copy of HW DB on any device.
 *
 * The function can receive three types of input:
 * @param[in] access_cmd -  get, get_next, get first
 * @param[in] key_filter -  filter types used on the mac_list - vid /mac / logical port
 *
 * @param[out] mac_list - mac record array pointer . On
 *                                        deletion, entry_type is DONT_CARE
 * @param[in,out] data_cnt - mac record array size
 * @param[in] fdb_lock - lock/unlock the FDB while accessing it.  1 - lock , 0 - don't lock
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EINVAL - invalid agrgument
 * @return -ENOENT - FDB Entry is not exist (incase of get)
 * @return -EPERM general error.
 */
int
ctrl_learn_api_uc_mac_addr_get(
    enum oes_access_cmd access_cmd,
    const struct fdb_uc_key_filter *key_filter,
    struct fdb_uc_mac_addr_params *mac_list,
    unsigned short  *data_cnt,
    const int fdb_lock
    )
{
    int err = 0;
    uint16_t record_cnt;
    uint64_t db_key;
    fdb_uc_mac_entry_t *mac_record_p = NULL;
    fdb_uc_mac_entry_t *tmp_record = NULL;
    void *list_cookie = NULL;
    record_cnt = 0;
    int mutex_acquired = 0;

    if (0 == is_ctrl_learn_initialized) {
        LOG(CL_LOG_ERR, "Ctrl learn DB is not initialized\n");
        err = -EPERM;
        goto end;
    }

    if (data_cnt == NULL) {
        err = -EINVAL;
        goto end;
    }

    if (*data_cnt == 0) {
        err = -EINVAL;
        goto end;
    }

    /* incase of filter by VID Validate range of vid */
    if (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID) {
        if (VID_MAX < key_filter->vid) {
            LOG(CL_LOG_ERR, "vid has invalid value (%u).\n",
                key_filter->vid);
            return -EINVAL;
        }
    }

    if (fdb_lock == 1) {
        /* acquire fdb lock */
        cl_spinlock_acquire(&ctrl_learn_fdb_lock);
        mutex_acquired = 1;
    }

    switch (access_cmd) {
    case    OES_ACCESS_CMD_GET:

        db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
            mac_list[0].mac_addr_params.mac_addr,
            mac_list[0].mac_addr_params.vid);
        err = fdb_uc_db_get_record_by_key( db_key, &mac_record_p);
        if ((err != 0)) {
            if (err != -ENOENT) {
                LOG(CL_LOG_ERR,
                    "Failed at fdb_uc_db_get_record_by_key err [%d]-[%s]\n",
                    err, strerror(
                        -err));
            }
            goto end;
        }

        mac_list[record_cnt].mac_addr_params.entry_type =
            mac_record_p->mac_params.entry_type;
        /*mac_list[record_cnt].mac_addr_params.entry_type = mac_record_p->type;*/
        mac_list[record_cnt].mac_addr_params.log_port =
            mac_record_p->mac_params.log_port;
        mac_list[record_cnt].mac_addr_params.vid =
            mac_record_p->mac_params.vid;
        mac_list[record_cnt].entry_type = mac_record_p->type;
        mac_list[record_cnt].cookie = mac_record_p->cookie;

        memcpy(&mac_list[record_cnt].mac_addr_params.mac_addr,
               &mac_record_p->mac_params.mac_addr,
               sizeof(mac_list[record_cnt].mac_addr_params.mac_addr));

        record_cnt++;
        err = 0;

        /* LOG(CL_LOG_DEBUG,
             "SX_ACCESS_CMD_GET-[vid:%u]-[mac:0x%" PRIx64 "]-[lport:0x%x]-[type:0x%x]\n", \
             mac_record_p->mac_params.vid,
             MAC_TO_U64(
                 mac_record_p->mac_params.mac_addr),  (unsigned int)mac_record_p->mac_params.log_port,
             (unsigned int)mac_record_p->type);
         */
        goto end;

    case    OES_ACCESS_CMD_GET_FIRST:
        if ((key_filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) ||
            (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
            err = fdb_uc_db_get_first_record_by_filter(key_filter,
                                                       &list_cookie,
                                                       &mac_record_p);
            if (err != 0) {
                goto end;
            }
        }
        else {
            err = fdb_uc_db_get_first_record( &mac_record_p);
            if (err != 0) {
                goto end;
            }
        }

        /*  LOG(CL_LOG_DEBUG, "OES_ACCESS_CMD_GET_FIRST \n");*/

        /* If entry not found */
        if (mac_record_p == NULL) {
            err = -ENOENT;
            goto end;
        }

        break;
    case    OES_ACCESS_CMD_GET_NEXT:

        db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
            mac_list[0].mac_addr_params.mac_addr,
            mac_list[0].mac_addr_params.vid);
        err = fdb_uc_db_get_record_by_key( db_key, &mac_record_p);
        if (err != 0) {
            goto end;
        }

        if ((key_filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) ||
            (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
            err = fdb_uc_db_get_first_record_by_filter(key_filter,
                                                       &list_cookie,
                                                       &tmp_record);
            if (err != 0) {
                goto end;
            }

            db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
                mac_list[0].mac_addr_params.mac_addr,
                mac_list[0].mac_addr_params.vid);
            err = fdb_uc_db_get_next_record_by_filter( key_filter,
                                                       list_cookie,
                                                       mac_record_p,
                                                       &tmp_record);
            if (err != 0) {
                goto end;
            }
        }
        else {
            err = fdb_uc_db_next_record(mac_record_p, &tmp_record);
            if (err != 0) {
                goto end;
            }
        }
        mac_record_p = tmp_record;

        /* If entry not found */
        if (mac_record_p == NULL) {
            err = -ENOENT;
            goto end;
        }

        /*LOG(CL_LOG_DEBUG, "OES_ACCESS_CMD_GET_NEXT \n");*/

        break;
    default:
        err = OES_STATUS_CMD_UNSUPPORTED;
        goto end;
    }

    while (mac_record_p != NULL) {
        mac_list[record_cnt].mac_addr_params.entry_type =
            mac_record_p->mac_params.entry_type;

        /*   LOG(CL_LOG_DEBUG,
               "[access_cmd:%u]-[vid:%u]-[mac:0x%" PRIx64 "]-[lport:0x%x]-[type:0x%x]\n",
               access_cmd,  mac_record_p->mac_params.vid,
               MAC_TO_U64(
                   mac_record_p->mac_params.mac_addr), (unsigned int)mac_record_p->mac_params.log_port,
               mac_record_p->type);
         */
        mac_list[record_cnt].mac_addr_params.vid =
            mac_record_p->mac_params.vid;

        memcpy(&mac_list[record_cnt].mac_addr_params.mac_addr,
               &mac_record_p->mac_params.mac_addr,
               sizeof(mac_list[record_cnt].mac_addr_params.mac_addr));

        mac_list[record_cnt].mac_addr_params.log_port =
            mac_record_p->mac_params.log_port;
        mac_list[record_cnt].mac_addr_params.entry_type =
            mac_record_p->mac_params.entry_type;
        mac_list[record_cnt].entry_type = mac_record_p->type;
        mac_list[record_cnt].cookie = mac_record_p->cookie;

        record_cnt++;

        if (record_cnt >= *data_cnt) {
            break;
        }

        if ((key_filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) ||
            (key_filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
            db_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(
                mac_list[0].mac_addr_params.mac_addr,
                mac_list[0].mac_addr_params.vid);
            err = fdb_uc_db_get_next_record_by_filter( key_filter,
                                                       list_cookie,
                                                       mac_record_p,
                                                       &tmp_record);
            if (err != 0) {
                goto end;
            }
        }
        else {
            err = fdb_uc_db_next_record(mac_record_p, &tmp_record);
            if (err != 0) {
                goto end;
            }
        }
        mac_record_p = tmp_record;
    }

end:
    *data_cnt = record_cnt;
    if ((fdb_lock == 1) && (mutex_acquired == 1)) {
        cl_spinlock_release(&ctrl_learn_fdb_lock);
    }
    return err;
}

/**
 *  Register mac notification callback.
 *  The callback will approve/deny each MAC notification.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPERM general error.
 */
int
ctrl_learn_register_notification(struct ctrl_learn_notify_params* params,
                                 ctrl_learn_notification_func notification_cb)
{
    int err = 0;

    if (is_ctrl_learn_start == 0) {
        LOG(CL_LOG_WARN,
            "Warning cannot register notification since Ctrl learn is not started yet\n");
        err = -EPERM;
        goto out;
    }

    if (params != NULL) {
        err = ctrl_learn_hal_fdb_notify_params_set(ctrl_learn_br_id, params);
        if (err != 0) {
            LOG(CL_LOG_ERR,
                "Failed at ctrl_learn_hal_fdb_notify_params_set err [%d] - [%s]\n",
                err, strerror(-err));
            goto out;
        }
    }

    ctrl_learn_notification_cb = notification_cb;

out:
    return err;
}

/**
 *  Unregister mac notification callback from control learn lib
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int
ctrl_learn_unregister_notification_cb(void)
{
    ctrl_learn_notification_cb = NULL;

    return 0;
}

/**
 *  Register init and deinit mac address cookie callback.
 *  The callback will init/deinit each MAC cookie.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int
ctrl_learn_register_init_deinit_mac_addr_cookie_cb(
    ctrl_learn_mac_addr_cookie_init_deinit_func init_deinit_cb)
{
    ctrl_learn_init_deinit_cb = init_deinit_cb;

    return 0;
}

/**
 *  Unregister init and deinit mac address cookie callback.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int
ctrl_learn_unregister_init_deinit_mac_addr_cookie_cb(void)
{
    ctrl_learn_init_deinit_cb = NULL;

    return 0;
}

/**
 *  lock the FDB and call the user callback.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int
ctrl_learn_api_get_uc_db_access( ctrl_learn_user_func cb, void *user_data)
{
    int err = 0;
    /*Take mutex*/
    cl_spinlock_acquire(&ctrl_learn_fdb_lock);

    err = cb(user_data);

    /*Give mutex*/
    cl_spinlock_release(&ctrl_learn_fdb_lock);
    return err;
}

/**
 *  Register cookie compare callback.
 *  The callback will be used to compare between two cookies.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int
ctrl_learn_register_coockie_cmp_func(ctrl_learn_cmp_func cmp_cb)
{
    UNUSED_PARAM(cmp_cb);

    return 0;
}
