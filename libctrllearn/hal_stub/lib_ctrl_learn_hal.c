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
#include "lib_ctrl_learn_hal.h"
#include "oes_types.h"
#include "errno.h"

/************************************************
 *  Global variables
 ***********************************************/
/************************************************
 *  Local function declarations
 ***********************************************/

/************************************************
 *  Function implementations
 ***********************************************/


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
int ctrl_learn_hal_fdb_uc_mac_addr_set(
    enum oes_access_cmd access_cmd, const int br_id,struct fdb_uc_mac_addr_params *mac_entry,
    unsigned short * mac_cnt){

    (void) access_cmd;
    (void) br_id;
    (void) mac_entry;
    (void) mac_cnt;

    return 0;
}

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
    const int br_id,
    struct ctrl_learn_notify_params* params){

    (void) br_id;
    (void) params;

    return 0;
}
