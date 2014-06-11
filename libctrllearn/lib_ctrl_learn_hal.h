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

#ifndef LIB_CTRL_LEARN_HAL_H_
#define LIB_CTRL_LEARN_HAL_H_

/**
 *  This function adds UC MAC entries in the FDB.
 *  In case the operation failed on one entry (or more), an error will be
 *  returned, mac_entry_list_p will store those entries, and their quantity
 *  will be stored in mac_cnt.
 *  If the operation finished successfully: SUCCESS will be returned, and
 *  mac_cnt and mac_entry_list_p won't be changed.
 *
 * @param[in] access_cmd - add/ delete
 * @param[in] br_id -  Bridge id
 * @param[in,out] mac_entry_p - mac record arry pointer . On
 *       deletion, entry_type is DONT_CARE
 * @param[in,out] mac_cnt - mac record arry size
 * @param[in] originator_cookie - originator cookie pass to notification callback function
 * @param[in] fdb_lock - lock/unlock the FDB while accessing it.  1 - lock , 0 - don't lock
 *
 * @return 0 - Operation completes successfully,
 * @return -EINVAL - invalid argument
 * @return -EPERM general error.
 */
int ctrl_learn_hal_fdb_uc_mac_addr_set(
    enum oes_access_cmd access_cmd, const int br_id,
    struct fdb_uc_mac_addr_params *mac_entry, unsigned short * mac_cnt);

/**
 *  This function sets FDB notify parameters.
 *
 * @param[in] br_id -  Bridge id
 * @param[in] params - new notify params
 *
 * @return 0 if operation completes successfully.
 * @return -EINVAL - if parameters are invalid
 * @return -EPERM general error.
 */
int ctrl_learn_hal_fdb_notify_params_set(
    const int br_id, struct ctrl_learn_notify_params* params);

#endif /* LIB_CTRL_LEARN_HAL_H_ */
