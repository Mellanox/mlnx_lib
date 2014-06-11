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

#ifndef __UC_FDB_DB_FLTR_H__
#define __UC_FDB_DB_FLTR_H__

#include <netinet/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
#include "oes_types.h"
#include "lib_ctrl_learn_uc_db.h"
#include "lib_ctrl_learn.h"


#ifdef __UC_FDB_DB_FLTR_C__

/************************************************
 *  Local Defines
 ***********************************************/
/* make key unique*/
#define BUILD_PORT_KEY(port)          (uint64_t)((uint64_t)port | \
                                                 (0x1000000000000ULL))

#define BUILD_PORT_VID_KEY(port, vid) (uint64_t)((uint64_t)port | \
                                                 ((uint64_t)(vid) << 32) | \
                                                 (0x2000000000000ULL));

#define CHECK_FLTRS_INIT_DONE    if (!filters_initiated) {                    \
        err = -EPERM;                                                      \
        CL_LOG(CL_LOG_ERR,  " filter DB not initialized. err = %d\n", err); \
        goto bail;                             \
}

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

struct fdb_fltr_entry {
    cl_map_item_t map_item;
    cl_qlist_t head;       /* head of the list that handles  mac_entries with same filter */
} fdb_fltr_entry;


#endif

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/


/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 * This function inits filtering database
 *
 * @param[in] log_func - log callback function
 * @return 0 if operation completes successfully
 * @return -ENOMEM if pool was not allocated

 */
int fdb_uc_db_filter_init(ctrl_learn_log_cb log_func);

/**
 * This function de-inits filtering database
 *
 * @return     0   if operation completes successfully
 */
int fdb_uc_db_filter_deinit(void);

/**
 * This function adds MAC entry to filtering list(s)
 *
 * @param[in] mac_entry pointer to mac entry added to the main DB

 * @return 0 if operation completes successfully.
 * @return -ENOENT in case pointer to mac_entry = NULL.
 */
int fdb_uc_db_filter_add_entry(fdb_uc_mac_entry_t * mac_entry);

/**
 * This function adds/removes MAC entry to/from filtering list(s) in case modified mac entry
 *
 * @param[in] old_mac_entry_p pointer to old mac entry
 * @param[in] new_mac_entry_p pointer to modified mac entry
 *
 * @return 0 if operation completes successfully.
 * @return -ENOENT in case pointer to old_mac_entry or new_mac_entry = NULL.
 */
int fdb_uc_db_filter_modified_entry(fdb_uc_mac_entry_t * old_mac_entry,
                                    fdb_uc_mac_entry_t * new_mac_entry);

/**
 * This function removes MAC entry from filtering list(s)
 *
 * @param[in]  mac_entry  pointer to  mac entry that removed from DB
 * @return 0 if operation completes successfully.
 */
int fdb_uc_db_filter_delete_entry(fdb_uc_mac_entry_t * mac_entry);

/**
 * This function gets first mac entry by filter
 *
 * @param[in]   filter       pointer to  filter structure
 * @param[out]  mac_item_pp  pointer to pointer to result mac entry
 * @param[out]  list_cookie  pointer to pointer to list handle - must be conveyed
 *                           to fdb_uc_db_filter_get_next_entry
 *
 * @return 0 if operation completes successfully.
 * @return -EPERM in case of logical errors.
 *
 */
int fdb_uc_db_filter_get_first_entry(const struct fdb_uc_key_filter * filter,
                                     void ** list_cookie,
                                     fdb_uc_mac_entry_t **mac_item_pp);

/**
 * This function gets next mac entry by filter
 *
 * @param[in]  filter        pointer to  filter structure
 * @param[in]  list_cookie   pointer to list handle - received from fdb_uc_db_filter_get_first_entry function
 *                           used for definition of end of the list
 * @param[in]  mac_item_p    pointer to current mac entry
 * @param[out] mac_item_pp   pointer to pointer to next mac entry or NULL if mac_item_p is last in the list
 *
 * @return 0 if operation completes successfully.
 * @return -EPERM in case of logical errors.
 */
int fdb_uc_db_filter_get_next_entry(const struct fdb_uc_key_filter  * filter,
                                    void                      * list_cookie,
                                    fdb_uc_mac_entry_t        * mac_item_p,
                                    fdb_uc_mac_entry_t        ** mac_item_pp);

/* Getters for debug purposes */

/**
 * This function returns number of mac entries  in the specific vlan list
 *
 * @param[in]  vid - vlan filter value
 * @@param[out] count number of free elements in the pool
 * @return number of mac entries in the specific vlan list.
 */
int fdb_uc_db_filter_get_number_of_vid_entries(unsigned short vid,
                                               uint32_t *count);

/**
 * This function returns number of mac entries  in the specific port list
 *
 * @param[in]   port - port filter value
 * @@param[out] count number of free elements in the pool
 * @return number of mac entries in the specific port list.
 */
int fdb_uc_db_filter_get_number_of_port_entries(unsigned long port,
                                                uint32_t *count);

/**
 * This function returns number of mac entries  in the specific port+vlan list
 *
 * @param[in]   port - port filter value
 * @param[in]   vlan - vlan filter value
 * @@param[out] count number of free elements in the pool
 * @return number of mac entries  in the specific port+vlan list.
 */
int fdb_uc_db_filter_get_number_of_port_vid_entries( unsigned long port,
                                                     unsigned short vid,
                                                     uint32_t *count);

/**
 * This function returns number of free elements in the pool used for qmap allocations
 *
 * @@param[out] count number of free elements in the pool.
 * @return error in case filters DB not initialized
 */

int fdb_uc_db_filter_get_free_pool_count(uint32_t *count);

/**
 *  This function set the module verbosity level
 *
 *  @param[in] verbosity_level - module verbosity level
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int fdb_uc_db_filter_log_verbosity_set(int verbosity_level);

#endif /* __UC_FDB_DB_FLTR_H__ */
