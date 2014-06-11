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

#ifndef __UC_FDB_DB_H__
#define __UC_FDB_DB_H__

#include <netinet/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
#include "oes_types.h"
#include "lib_ctrl_learn.h"

#ifdef __UC_FDB_DB_C__

/************************************************
 *  Local Defines
 ***********************************************/
#define CHECK_FDB_INIT_DONE                    \
    if (!fdb_initiated) {                         \
        err = -EPERM;                          \
        CL_LOG(CL_LOG_ERR,  " FDB not initialized. err = %d\n", err); \
        goto bail;                             \
    }

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

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
#define FDB_UC_CONVERT_MAC_VLAN_TO_KEY(mac_addr, vid)  \
    (uint64_t)((MAC_TO_U64(mac_addr)) | ((uint64_t)(vid) << 48))


#define SX_FDB_UC_TYPE_DYNAMIC                  0x01
#define SX_FDB_UC_TYPE_STATIC                   0x02

/**
 * fdb_uc_mac_entry_t structure is used to store mac records
 * in SW DB
 */
typedef struct fdb_uc_mac_entry {
    cl_map_item_t map_item;
    struct oes_fdb_uc_mac_addr_params mac_params;
    void *  cookie; /* user's data */
    enum fdb_uc_mac_entry_type type;       /* type of MAC entry : dynamic or static */
    /* filtering list entries */
    cl_list_item_t vid_fltr_entry;
    cl_list_item_t port_fltr_entry;
    cl_list_item_t vid_port_fltr_entry;
} fdb_uc_mac_entry_t;

/**
 * This function initializes SW DB
 *
 * param[in] log_cb -  log callback supplied by user
 * @return 0 operation completes successfully
 * @return -EPERM   general error
 */
int fdb_uc_db_init(ctrl_learn_log_cb log_cb);

/**
 * This function de-inits the DB
 *
 * @return 0 operation completes successfully
 * @return -EPERM   general error
 */
int fdb_uc_db_deinit(void);

/* This function adds a mac entry to the fdb
 *
 * @param[in] mac_entry_p - input entry
 * @return 0  operation completes successfully
 * @return -ENOMEM not enough memory
 * @return -ENOENT general error
 */
int fdb_uc_db_add_record(fdb_uc_mac_entry_t *mac_entry_p);

/**
 * This function removes the entry from DB
 * @param[in] mac_item_p - input entry
 *
 * @return 0  operation completes successfully
 * @return -EPERM   general error
 */
int fdb_uc_db_delete_record(fdb_uc_mac_entry_t *mac_item_p);

/**
 * This function returns the pointer for first entry in DB by specific filter
 *
 * @param[in] filter        - pointer to filters
 * @param[out] list_cookie  - cookie of specific filter
 * @param[out] mac_item_pp  - pointer to first entry or NULL
 *
 * @return 0 operation completes successfully
 * @return   -ENOENT in case of null pointer
 * @return   -EINVAL in case invalid filter
 * @return   -EPERM  in case wrong value
 */
int fdb_uc_db_get_first_record_by_filter(
    const struct fdb_uc_key_filter * filter,
    void                    ** list_cookie,
    fdb_uc_mac_entry_t      **mac_item_pp);

/**
 * This function returns the pointer for next entry in DB by filter
 *
 * @param[in]  filter    - pointer to filters
 * @param[in]  list_cookie  - cookie of specific filter
 * @param[in]  mac_item_p - pointer to current entry
 * @param[out] mac_item_pp - pointer to next entry or NULL if mac_item_p is the last entry
 *
 * @return 0 -operation completes successfully
 * @return   -ENOENT in case of null pointer
 * @return   -EINVAL in case invalid filter
 * @return   -EPERM  in case general error
 */
int fdb_uc_db_get_next_record_by_filter(
    const struct fdb_uc_key_filter * filter,
    void                     * list_cookie,
    fdb_uc_mac_entry_t       * mac_item_p,
    fdb_uc_mac_entry_t       **mac_item_pp);

/**
 * This function returns the pointer for first entry in DB
 *
 * @param[out] mac_item_pp - pointer to entry
 *
 * @return 0 operation completes successfully
 * @return   -EPERM  in case general error
 */
int fdb_uc_db_get_first_record(fdb_uc_mac_entry_t **mac_item_pp);

/**
 * This function returns the pointer to the entry with given key
 *
 * @param[in] mac_key - mac DB key
 * @param[out] mac_item_pp - pointer to entry
 *
 * @return 0 operation completes successfully
 * @return   -ENOENT  in case general error
 */
int fdb_uc_db_get_record_by_key(
    uint64_t mac_key, fdb_uc_mac_entry_t **mac_item_p);

/**
 * This function returns the pointer to next entry that comes
 * after the one with the given key
 * @param[in] fdb_map - mac DB
 * @param[in] mac_key - mac DB key
 * @param[out] mac_item_pp - pointer to entry
 *
 * @return 0 operation completes successfully
 * @return   -ENOENT  in case  entry not exists
 */
int fdb_uc_db_get_next_record_by_key(
    cl_qmap_t *fdb_map, uint64_t mac_key, fdb_uc_mac_entry_t **mac_item_pp);

/**
 * This function returns the pointer to next entry that comes
 * after the given one
 * @param[in] mac_item_p - input entry
 * @param[out] return_item_pp - pointer to entry
 *
 * @return 0  operation completes successfully
 * @return -EPERM   general error
 */
int fdb_uc_db_next_record(
    fdb_uc_mac_entry_t *mac_item_p, fdb_uc_mac_entry_t **return_item_pp);

/**
 * This function returns the size of the DB
 *
 * @param[out] db_size - DB size
 *
 * @return 0 operation completes successfully
 */
int fdb_uc_db_get_size(uint32_t *db_size);

/**
 * This function returns number of free items in the DB pool
 *
 * @param[out]  count - number of free pools
 *
 * @return 0
 */
int fdb_uc_db_get_free_pool_count(uint32_t * count);

/**
 *  This function set the module verbosity level
 *
 *  @param[in] verbosity_level - module verbosity level
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int fdb_uc_db_log_verbosity_set(int verbosity_level);

#endif /* __FDB_UC_IMPL_H_INCL__ */
