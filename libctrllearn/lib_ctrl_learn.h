/*
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2014.-- ALL RIGHTS RESERVED.
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

#ifndef LIB_CTRL_LEARN_H_
#define LIB_CTRL_LEARN_H_

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
#include <oes_types.h>
#include <complib/sx_log.h>

/************************************************
 *  Defines
 ***********************************************/

#define CTRL_LEARN_FDB_NOTIFY_SIZE_MAX 670


/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

typedef sx_log_cb_t ctrl_learn_log_cb;
/**
 * user callback that access control learning FDB
 */
typedef int (*ctrl_learn_user_func)(void* user_data);

/**
 * Compare cookie entry function callback
 * The function compares two cookies
 *  @return 0  both cookies are equal
 *  @return 1  cookie1 has greater value than cookie2
 *  @return -1 cookie2 has greater value than cookie1.
 */
typedef int (*ctrl_learn_cmp_func)(void  * cookie1, void * cookie2);


enum cookie_op {
    COOKIE_OP_INIT,
    COOKIE_OP_DEINIT
};

/**
 * fdb_key_filter_field_valid enumerated type is set key filter field valid or no valid
 *
 */
enum fdb_key_filter_field_valid {
    FDB_KEY_FILTER_FIELD_NOT_VALID,
    FDB_KEY_FILTER_FIELD_VALID,
};

/**
 * FDB get key filter structure
 */
struct fdb_uc_key_filter {
    enum fdb_key_filter_field_valid filter_by_vid;  /**< If key field is valid then filter by vid*/
    unsigned short vid;                 /**< VID */
    enum fdb_key_filter_field_valid filter_by_log_port; /**< If key field is valid then filter by logical port*/
    unsigned long log_port;                        /**< Logical port */
};

/**
 * FDB notification parameters structure for controlled learning
 *
 * interval_units: The maximum elapsed time between two FDB events in units of
 * polling interval (set by sx_api_fdb_polling_interval_set). Must be a positive integer
 * example: interval_units==3, polling interval==0.2: every 0.6 seconds (3*0.2)
 * FDB event will be sent.
 * size_threshold: The maximum amount of FDB entries to aggregate. When aggregated
 * size achieves size_threshold, FDB event will be generated. Valid range: [1 - CTRL_LEARN_FDB_NOTIFY_SIZE_MAX]
 */

struct ctrl_learn_notify_params {
    uint32_t interval_units;
    uint32_t size_threshold;
};

enum ctrl_learn_notify_decision {
    CTRL_LEARN_NOTIFY_DECISION_APPROVE,
    CTRL_LEARN_NOTIFY_DECISION_DENY,
};

/**
 * FDB MAC Entries types
 */
enum fdb_uc_mac_entry_type {
    FDB_UC_STATIC,
    FDB_UC_REMOTE,
    FDB_UC_NONAGEABLE,
    FDB_UC_AGEABLE,
    FDB_ENTRY_TYPE_MIN = FDB_UC_STATIC,
    FDB_ENTRY_TYPE_MAX = FDB_UC_AGEABLE
};


struct ctrl_learn_fdb_notify_record {
    enum oes_fdb_event_type event_type;
    struct oes_event_fdb oes_event_fdb;
    enum ctrl_learn_notify_decision decision;
    enum fdb_uc_mac_entry_type entry_type;
    void *cookie; /**< FDB entry cookie*/
};

struct ctrl_learn_fdb_notify_data {
    uint32_t records_num;
    struct ctrl_learn_fdb_notify_record records_arr[
        CTRL_LEARN_FDB_NOTIFY_SIZE_MAX];
};

enum ctrl_learn_notify_result {
    CTRL_LEARN_NOTIFY_RESULT_APPROVE,
    CTRL_LEARN_NOTIFY_RESULT_DENY,
};


struct fdb_uc_mac_addr_params {
    struct oes_fdb_uc_mac_addr_params mac_addr_params;    /**< MAC address */
    enum fdb_uc_mac_entry_type entry_type;
    void *cookie;
};

/**
 * Notification of mac and approval callback
 * The function update notif_records with the decision (approve / deny)
 */
typedef int (*ctrl_learn_notification_func)(struct ctrl_learn_fdb_notify_data*
                                            notif_records,
                                            const void* originator_cookie);

/**
 * Init/de init cookie
 */
typedef int (*ctrl_learn_mac_addr_cookie_init_deinit_func)( enum cookie_op,
                                                            void** cookie);

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 *  This function set the module verbosity level
 *
 *  @param[in] verbosity_level - module verbosity level
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int ctrl_learn_log_verbosity_set(int verbosity_level);

/**
 *  This function get the log verbosity level of the module
 *
 *  @param[out] verbosity_level - module verbosity level
 *
 *  @return 0 when successful.
 *  @return -EINVAL - Unsupported verbosity_level
 */
int ctrl_learn_log_verbosity_get(int * verbosity_level);

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
int ctrl_learn_init(int br_id, int enable_multiple_fdb_notif,
                    ctrl_learn_log_cb logging_cb);

/**
 *  This functions start the Control learning library. It start listening to FDB event
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int ctrl_learn_start(void);

/**
 *  Stop Control learning functionality
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int ctrl_learn_stop(void);

/**
 *  This function de-inits the Control learning library - cleanup memory,structures,etc..
 *
 *  @return 0 when successful.
 *  @return -EPERM general error
 */
int ctrl_learn_deinit(void);

/**
 *  This function adds UC MAC entries in the FDB.
 *  In case the operation failed on one entry (or more), an error will be
 *  returned, mac_entry_list_p will store those entries, and their quantity
 *  will be stored in mac_cnt.
 *  If the operation finished successfully: SUCCESS will be returned, and
 *  mac_cnt and mac_entry_list_p won't be changed.
 *
 * @param[in] access_cmd - add/ delete
 * @param[in,out] mac_entry_p - mac record arry pointer . On
 *       deletion, entry_type is DONT_CARE
 * @param[in,out] mac_cnt - mac record arry size
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 * @param[in] fdb_lock - lock/unlock the FDB while accessing it.  1 - lock , 0 - don't lock
 *
 * @return 0 - Operation completes successfully,
 * @return -EINVAL - Unsupported verbosity_level
 * @return OES_STATUS_ERROR general error.
 */
int ctrl_learn_api_fdb_uc_mac_addr_set(
    enum oes_access_cmd access_cmd, struct fdb_uc_mac_addr_params *mac_entry,
    unsigned short * mac_cnt, const void *originator_cookie,
    const int fdb_lock);

/**
 * This function deletes all of the FDB table.
 *
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EPRM general error.
 */
int ctrl_learn_api_fdb_uc_flush_set(const void *originator_cookie);

/**
 *  This function deletes the FDB table entries that are related
 *  to a flushed port.
 *
 * @param[in] log_port- logical port ID
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 *
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EINVAL - invalid agrgument
 * @return -EPRM general error.
 *
 */
int ctrl_learn_api_fdb_uc_flush_port_set(unsigned long log_port,
                                         const void *originator_cookie);

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
    unsigned short vid, const void *originator_cookie);


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
    unsigned short vid, unsigned long log_port, const void *originator_cookie);
/**
 * This function reads MAC entries from the SW FDB table, which
 * is exact copy of HW DB on any device.
 *
 * function can receive three types of input:
 *     1) get information for specific mac address ,user
 *      should insert the certain mac address as the
 *      firstmac_entry_list  element in the mac_entry_list array
 *      ,mac_cnt should be equal to 1, access_cmd should be
 *      OES_ACCESS_CMD_GET
 *
 *   - 2) get a list of first n mac entries ,user
 *      should provide an empty  mac_entry_list  array mac_cnt
 *      should be equal to n,access_cmd should be
 *      OES_ACCESS_CMD_GET_FIRST
 *
 *   - 3) get a list of n  mac entries  which comes after
 *      given mac address(it does not have to exist) user should
 *      insert the specific  mac address  as the first
 *      mac_entry_list element in the mac_entry_list array ,
 *      mac_cnt should be equal to n, access_cmd should be
 *      OES_ACCESS_CMD_GET_NEXT
 *
 * The function can receive three types of input:
 * @param[in] access_cmd -  GET/GET NEXT/GET FIRST.
 * @param[in] key_filter -  filter types used on the mac_list - vid /mac / logical port
 *
 * @param[out] mac_list - mac record array pointer . On
 *                                        deletion, entry_type is DONT_CARE
 * @param[in,out] data_cnt - mac record array size
 * @param[in] fdb_lock - lock/unlock the FDB while accessing it.  1 - lock , 0 - don't lock
 * @return 0 if operation completes successfully,otherwise ERROR
 * @return -EINVAL - invalid agrgument
 * @return -ENOENT - FDB Entry does not exist (incase of get)
 * @return -EPRM general error.
 */
int ctrl_learn_api_uc_mac_addr_get(
    enum oes_access_cmd access_cmd, const struct fdb_uc_key_filter *key_filter,
    struct fdb_uc_mac_addr_params *mac_list, unsigned short  *data_cnt, const int fdb_lock
    );

/**
 *  Register mac notification callback.
 *  The callback will approve/deny each MAC notification.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int ctrl_learn_register_notification(struct ctrl_learn_notify_params* params,
                                     ctrl_learn_notification_func notification_cb);

/**
 *  Unregister mac notification callback from control learn lib
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int ctrl_learn_unregister_notification_cb(void);

/**
 *  Register init and deinit mac address cookie callback.
 *  The callback will init/deinit each MAC cookie.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int ctrl_learn_register_init_deinit_mac_addr_cookie_cb(
    ctrl_learn_mac_addr_cookie_init_deinit_func init_deinit_cb);


/**
 *  lock the FDB and call the user callback.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int ctrl_learn_api_get_uc_db_access( ctrl_learn_user_func cb, void *user_data);


/**
 *  Unregister init and deinit mac address cookie callback.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int ctrl_learn_unregister_init_deinit_mac_addr_cookie_cb(void);
/**
 *  Register cookie compare callback.
 *  The callback will approve/deny each MAC notification.
 *
 *  @return 0 - Operation completes successfully
 *  @return -EPRM general error.
 */
int ctrl_learn_register_coockie_cmp_func(ctrl_learn_cmp_func cmp_cb);


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
    const struct fdb_uc_mac_addr_params *mac_entry, unsigned short mac_cnt);

#endif /* LIB_CTRL_LEARN_H_ */
