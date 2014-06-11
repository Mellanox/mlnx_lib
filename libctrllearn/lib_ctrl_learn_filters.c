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

#define  __UC_FDB_DB_FLTR_C__

#include <stdio.h>
#include <complib/sx_log.h>
#include <complib/cl_mem.h>
#include <complib/cl_qmap.h>
#include <complib/cl_pool.h>
#include <complib/cl_qlist.h>
#include "lib_ctrl_learn_defs.h"
#include "lib_ctrl_learn_filters.h"
#include  "/usr/include/asm-generic/errno-base.h"



/************************************************
 *  Local definitions
 ***********************************************/
#undef  __MODULE__
#define __MODULE__ CTRL_LEARN_FILTER_DB

/************************************************
 *  Global variables
 ***********************************************/
static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) =
    SX_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local variables
 ***********************************************/
static cl_qmap_t fbd_filter_map;           /* qmap used for store all filters of "port" type and "port_vlan" type*/
static cl_pool_t fdb_fltr_pool;            /* used for allocations entries for fbd_filter_map  */
static cl_qlist_t fdb_vlan_list[MAX_VLAN_ENTRIES]; /* array of list heads for the vlan filter store*/
static ctrl_learn_log_cb ctrl_learn_logging_cb;      /* log callback */
static int filters_initiated;            /* static flag risen when database initialized*/

/************************************************
 *  Local function declarations
 ***********************************************/
static int link_entry_to_fltr_list(fdb_uc_mac_entry_t * mac_entry,
                                   uint64_t map_key, uint32_t parent_offset);
static int delink_entry_from_fltr_list(fdb_uc_mac_entry_t * mac_entry,
                                       uint64_t map_key,
                                       uint32_t parent_offset);
static int fdb_uc_db_filter_get_first_record(
    struct fdb_fltr_entry   **fltr_entry);

static int fdb_uc_db_filter_destroy();

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function inits  filtering database
 * @return     0   if operation completes successfully
 * @return -ENOMEM if pool was not allocated

 */
int
fdb_uc_db_filter_init(ctrl_learn_log_cb log_cb)
{
    int i;
    int err = 0;

    if (filters_initiated) {
        goto bail;
    }

    for (i = 0; i < MAX_VLAN_ENTRIES; i++) {
        cl_qlist_init(&fdb_vlan_list[i]);
    }

    cl_qmap_init(&fbd_filter_map);

    err = cl_pool_init(&fdb_fltr_pool,
                       MIN_FILTER_ENTRIES, MAX_FILTER_ENTRIES,
                       0,
                       sizeof(struct fdb_fltr_entry), NULL, NULL, NULL);
    if (err) {
        err = -ENOMEM;
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    ctrl_learn_logging_cb = log_cb;
    filters_initiated = 1;
    CL_LOG(CL_LOG_NOTICE, "Filtering DB inited \n");
bail:
    return err;
}

/**
 *  This function set the module verbosity level
 *
 *  @param[in] verbosity_level - module verbosity level
 *
 *  @return 0 when successful
 *  @return -EPERM general error
 */
int
fdb_uc_db_filter_log_verbosity_set(int verbosity_level)
{
    int err = 0;

    LOG_VAR_NAME(__MODULE__) = verbosity_level;

    return err;
}

/**
 * This function returns the pointer for first entry in DB
 * @param[out] fltr_entry - pointer to entry

 * @return 0  operation completes successfully
 */
int
fdb_uc_db_filter_get_first_record(struct fdb_fltr_entry   **fltr_entry)
{
    int err = 0;
    cl_map_item_t *map_item_p = NULL;
    *fltr_entry = NULL;

    CHECK_FLTRS_INIT_DONE;

    if (CL_QMAP_HEAD(map_item_p, &fbd_filter_map)) {
        if (!CL_QMAP_END(map_item_p, &fbd_filter_map)) {
            *fltr_entry = CL_QMAP_PARENT_STRUCT(struct fdb_fltr_entry);
        }
    }
bail:
    return err;
}


/**
 * This function cleans the filtering DB
 * @return 0  operation completes successfully
 */
int
fdb_uc_db_filter_destroy()
{
    int err = 0;
    struct fdb_fltr_entry   *fltr_entry;
    CHECK_FLTRS_INIT_DONE;

    err = fdb_uc_db_filter_get_first_record(&fltr_entry);
    if (err) {
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    while (fltr_entry) {
        cl_qmap_remove_item(&fbd_filter_map,  &fltr_entry->map_item);
        cl_pool_put(&fdb_fltr_pool, fltr_entry);
        err = fdb_uc_db_filter_get_first_record(&fltr_entry);
        if (err) {
            CL_LOG(CL_LOG_ERR, " err = %d\n", err);
            goto bail;
        }
    }
bail:
    return err;
}

/**
 *  This function de-inits  filtering database
 *  @return     0   if operation completes successfully
 */
int
fdb_uc_db_filter_deinit()
{
    int err = 0;

    CHECK_FLTRS_INIT_DONE;
    /*1. remove all entries from the maps */
    err = fdb_uc_db_filter_destroy();
    if (err) {
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    /*2. destroy pools*/
    cl_pool_destroy(&fdb_fltr_pool);
    filters_initiated = 0;
    CL_LOG(CL_LOG_NOTICE, "Filtering DB deinited \n");
bail:
    return err;
}


/**
 *  This function adds MAC entry to filtering list(s)
 *
 * @param[in] mac_entry pointer to mac entry added to the main DB

 * @return 0 if operation completes successfully.
 * @return -ENOENT in case pointer to mac_entry = NULL.
 */
int
fdb_uc_db_filter_add_entry(fdb_uc_mac_entry_t * mac_entry)
{
    unsigned short vid;
    unsigned long port;
    int err = 0;
    uint64_t map_key;

    CHECK_FLTRS_INIT_DONE;

    if (!mac_entry) {
        err = -ENOENT;
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    vid = mac_entry->mac_params.vid;
    port = mac_entry->mac_params.log_port;

    /* 1. link to vid filters*/
    if (vid >= MAX_VLAN_ENTRIES) {
        err = -EPERM;
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    cl_qlist_insert_tail(&(fdb_vlan_list[vid]),
                         (cl_list_item_t *) ((uint8_t *)mac_entry +
                                             offsetof(fdb_uc_mac_entry_t,
                                                      vid_fltr_entry )));
    /* 2. link to port filters*/
    uint64_t map_port = port;
    map_key = BUILD_PORT_KEY(map_port);
    err =
        link_entry_to_fltr_list(mac_entry, map_key,
                                offsetof(fdb_uc_mac_entry_t,
                                         port_fltr_entry ));
    if (err) {
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    /* 3. link to port+vid filters*/
    map_key = BUILD_PORT_VID_KEY(port, vid);
    err =
        link_entry_to_fltr_list(mac_entry, map_key,
                                offsetof(fdb_uc_mac_entry_t,
                                         vid_port_fltr_entry ));
    if (err) {
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
bail:
    return err;
}


/**
 *  This function adds/removes MAC entry to/from filtering list(s) in case modified mac entry
 *
 * @param[in] old_mac_entry_p pointer to old mac entry
 * @param[in] new_mac_entry_p pointer to modified mac entry
 * @return 0 if operation completes successfully.
 * @return -ENOENT in case pointer to old_mac_entry or new_mac_entry = NULL.
 */
int
fdb_uc_db_filter_modified_entry( fdb_uc_mac_entry_t *old_mac_entry_p,
                                 fdb_uc_mac_entry_t *new_mac_entry_p)
{
    int err = 0;
    uint64_t map_key;
    unsigned long old_log_port, new_log_port;
    unsigned short vid;

    CHECK_FLTRS_INIT_DONE

    if (!old_mac_entry_p || !new_mac_entry_p) {
        err = -ENOENT;
        CL_LOG(CL_LOG_ERR, " null pointer. err = %d\n", err);
        goto bail;
    }
    old_log_port = old_mac_entry_p->mac_params.log_port;
    new_log_port = new_mac_entry_p->mac_params.log_port;
    vid = new_mac_entry_p->mac_params.vid;

    if (old_log_port != new_log_port) {
        /*CL_LOG("CL_LOG_NOTICE, filter_modified_entry\n");*/
        map_key = BUILD_PORT_KEY(old_log_port); /* remove entry from the old port filter list */
        err =
            delink_entry_from_fltr_list(new_mac_entry_p, map_key,
                                        offsetof(fdb_uc_mac_entry_t,
                                                 port_fltr_entry ));
        if (err) {
            goto bail;
        }
        map_key = BUILD_PORT_KEY(new_log_port); /* add entry to the new port filter list */
        err =
            link_entry_to_fltr_list(new_mac_entry_p, map_key,
                                    offsetof(fdb_uc_mac_entry_t,
                                             port_fltr_entry ));
        if (err) {
            goto bail;
        }

        map_key = BUILD_PORT_VID_KEY( old_log_port, vid); /* remove entry from the old port_vlan filter list */
        err =
            delink_entry_from_fltr_list(new_mac_entry_p, map_key,
                                        offsetof(fdb_uc_mac_entry_t,
                                                 vid_port_fltr_entry ));
        if (err) {
            goto bail;
        }
        map_key = BUILD_PORT_VID_KEY( new_log_port, vid); /* add entry to the new port_vlan filter list */
        err =
            link_entry_to_fltr_list(new_mac_entry_p, map_key,
                                    offsetof(fdb_uc_mac_entry_t,
                                             vid_port_fltr_entry ));
        if (err) {
            goto bail;
        }
    }

bail:
    return err;
}

/**
 *  This function removes MAC entry from filtering list(s)
 *
 * @param[in]  mac_entry  pointer to  mac entry that removed from DB
 * @return 0 if operation completes successfully.
 */
int
fdb_uc_db_filter_delete_entry(fdb_uc_mac_entry_t *mac_entry)
{
    uint64_t map_key;
    int err = 0;

    CHECK_FLTRS_INIT_DONE;

    if (mac_entry->vid_fltr_entry.p_next && mac_entry->vid_fltr_entry.p_prev) {
        cl_qlist_remove_item(&(fdb_vlan_list[mac_entry->mac_params.vid]),
                             (cl_list_item_t *) ((uint8_t *)mac_entry +
                                                 offsetof(fdb_uc_mac_entry_t,
                                                          vid_fltr_entry )));
        memset(&mac_entry->vid_fltr_entry, 0,
               sizeof(mac_entry->vid_fltr_entry));
    }
    map_key = BUILD_PORT_KEY(mac_entry->mac_params.log_port);
    err =
        delink_entry_from_fltr_list(mac_entry, map_key,
                                    offsetof(fdb_uc_mac_entry_t,
                                             port_fltr_entry ));
    if (err) {
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }

    map_key = BUILD_PORT_VID_KEY(mac_entry->mac_params.log_port,
                                 mac_entry->mac_params.vid);
    err =
        delink_entry_from_fltr_list(mac_entry, map_key,
                                    offsetof(fdb_uc_mac_entry_t,
                                             vid_port_fltr_entry ));

    if (err) {
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }

bail:
    return err;
}


/**
 *  This function links MAC entry to filtering list(s)
 *
 * @param[in]  mac_entry      pointer to  mac entry that added to DB
 * @param[in]  mac_key        key for qmap to find proper list head
 * @param[in]  parent_offset  offset to list_entry field in  mac entry
 *
 * @return 0 if operation completes successfully.
 */

int
link_entry_to_fltr_list(fdb_uc_mac_entry_t *mac_entry, uint64_t map_key,
                        uint32_t parent_offset )
{
    struct fdb_fltr_entry  *fltr_item_p = NULL;
    cl_map_item_t   *map_item_p = NULL;
    int err = 0;

    CHECK_FLTRS_INIT_DONE;

    if (CL_QMAP_KEY_EXISTS(&fbd_filter_map, map_key, map_item_p)) {  /* already existed list head for the filter - need only to add mac_entry to it*/
        fltr_item_p = CL_QMAP_PARENT_STRUCT(struct fdb_fltr_entry);

        if (!fltr_item_p) {
            err = ENOENT;
            CL_LOG(CL_LOG_ERR, " null pointer. err = %d\n", err);
            goto bail;
        }
    }
    else {  /* list head for this filter not exist - create it , insert to qmap first*/
        /* allocate entry, init list component of it and add entry to the map */
        fltr_item_p = (struct fdb_fltr_entry *)cl_pool_get(&fdb_fltr_pool);
        if (!fltr_item_p) {
            err = ENOMEM;
            CL_LOG(CL_LOG_ERR, " memory alloc fail. err = %d\n", err);
            goto bail;
        }
        MEM_CLR_P((fltr_item_p));
        cl_qlist_init(&fltr_item_p->head);
        cl_qmap_insert(&fbd_filter_map, map_key, &(fltr_item_p->map_item));
    }
    cl_qlist_insert_tail(&(fltr_item_p->head),
                         (cl_list_item_t *) ((uint8_t *)mac_entry +
                                             parent_offset));
    fltr_item_p->head.count = 0; /* count not supported on all list heads in the qmap  since delete performs in O(1)  not updating count*/

bail:
    return err;
}


/**
 *  This function de-links MAC entry from filtering list(s)
 *
 * @param[in]  mac_entry      pointer to  mac entry that added to DB
 * @param[in]  mac_key        key for qmap to find proper list head
 * @param[in]  parent_offset  offset to list_entry field in  mac entry
 *
 * @return 0 if operation completes successfully.
 * @return -EPERM in case of errord.
 *
 */
static int
delink_entry_from_fltr_list(fdb_uc_mac_entry_t *mac_entry, uint64_t map_key,
                            uint32_t parent_offset )
{
    int err = 0;
    cl_list_item_t  *list_item =
        (cl_list_item_t *) ((uint8_t *)mac_entry + parent_offset);
    struct fdb_fltr_entry  *fltr_item_p = NULL;
    cl_map_item_t   *map_item_p = NULL;
    int last_deleted = 0;

    CHECK_FLTRS_INIT_DONE;

    if (list_item->p_prev && (list_item->p_next == list_item->p_prev)) {
        /* need to delete head of the list*/
        last_deleted = 1;
    }
    if (list_item->p_next && list_item->p_prev) {
        __cl_primitive_remove(list_item);
        memset(list_item, 0, sizeof(cl_list_item_t));
    }
    else {
        err = -EPERM;
        CL_LOG(CL_LOG_ERR, " de-link err = %d\n", err);
        goto bail;
    }
    if (last_deleted) {
        if (CL_QMAP_KEY_EXISTS(&fbd_filter_map, map_key, map_item_p)) {  /* need to remove list head from the qmap*/
            fltr_item_p = CL_QMAP_PARENT_STRUCT(struct fdb_fltr_entry);
            cl_qmap_remove_item(&fbd_filter_map, &fltr_item_p->map_item);
            cl_pool_put(&fdb_fltr_pool, fltr_item_p);
        }
        else {
            err = -EPERM;
            CL_LOG(CL_LOG_ERR, " de-link err = %d\n", err);
        }
    }
bail:
    return err;
}


/**
 *  This function gets first mac entry by filter
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
int
fdb_uc_db_filter_get_first_entry(const struct fdb_uc_key_filter *filter,
                                 void **list_cookie,
                                 fdb_uc_mac_entry_t **mac_item_pp
                                 )
{
    int err = 0;
    uint64_t map_key;
    cl_map_item_t   *map_item_p = NULL;
    struct fdb_fltr_entry  *fltr_item_p = NULL;
    cl_list_item_t * head;
    uint32_t parent_offset = 0;

    CHECK_FLTRS_INIT_DONE;

    if (!list_cookie) {
        err = ENOENT;
        CL_LOG(CL_LOG_ERR, " null pointer. err = %d\n", err);
        goto bail;
    }
    if (!mac_item_pp) {
        err = ENOENT;
        CL_LOG(CL_LOG_ERR, " null pointer. err = %d\n", err);
        goto bail;
    }
    if (!filter) {
        err = ENOENT;
        CL_LOG(CL_LOG_ERR, " null pointer. err = %d\n", err);
        goto bail;
    }
    *list_cookie = NULL;
    if ((filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) &&
        (filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
        map_key = BUILD_PORT_VID_KEY(filter->log_port, filter->vid);
        parent_offset = offsetof(fdb_uc_mac_entry_t, vid_port_fltr_entry);
    }
    else if (filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) {
        map_key = BUILD_PORT_KEY(filter->log_port);
        parent_offset = offsetof(fdb_uc_mac_entry_t, port_fltr_entry);
    }
    else if (filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID) {
        if (cl_is_qlist_empty(&fdb_vlan_list[filter->vid])) {
            *mac_item_pp = NULL;
            goto bail;
        }
        head = cl_qlist_head(&(fdb_vlan_list[filter->vid]));
        *mac_item_pp = PARENT_STRUCT(head, fdb_uc_mac_entry_t, vid_fltr_entry);
        *list_cookie = (void **)head->p_prev; /* list_cookie  points to the tail of the list*/
        goto bail;
    }
    else {
        err = EINVAL;
        CL_LOG(CL_LOG_ERR, " invalid filter. err = %d\n", err);
        goto bail;
    }

    if (CL_QMAP_KEY_EXISTS(&fbd_filter_map, map_key, map_item_p)) {
        fltr_item_p = CL_QMAP_PARENT_STRUCT(struct fdb_fltr_entry);
        head = cl_qlist_head(&fltr_item_p->head);
        *mac_item_pp = (fdb_uc_mac_entry_t*) ((uint8_t*)head - parent_offset);
        *list_cookie = (void **)head->p_prev; /* list_cookie points to the tail of the list*/
    }
    else {
        *mac_item_pp = NULL;
    }
bail:
    return err;
}

/**
 * This function gets next mac entry by filter
 *
 * @param[in]   filter       pointer to  filter structure
 * @param[in]  list_cookie   pointer to list handle - received from fdb_uc_db_filter_get_first_entry function
 *                           used for definition of end of the list
 * @param[in]   mac_item_p  pointer to current mac entry
 * @param[out]  mac_item_pp  pointer to pointer to next  mac entry or NULL if  mac_item_p is last in the list
 *
 * @return 0 if operation completes successfully.
 * @return -EPERM in case of logical errors.
 */
int
fdb_uc_db_filter_get_next_entry(const struct fdb_uc_key_filter *filter,
                                void                    *list_cookie,
                                fdb_uc_mac_entry_t    *mac_item_p,
                                fdb_uc_mac_entry_t    **mac_item_pp)
{
    int err = 0;
    cl_list_item_t *cur_list_item = NULL, *next_list_item = NULL;
    uint32_t parent_offset = 0;

    CHECK_FLTRS_INIT_DONE;

    if (!filter) {
        err = -ENOENT;
        CL_LOG(CL_LOG_ERR, "null filter pointer.  err = %d\n", err);
        goto bail;
    }
    if (!mac_item_p) {
        err = -ENOENT;
        CL_LOG(CL_LOG_ERR, "null prev. pointer.  err = %d\n", err);
        goto bail;
    }
    if (!list_cookie) {
        err = -ENOENT;
        CL_LOG(CL_LOG_ERR, "null list_cookie pointer.  err = %d\n", err);
        goto bail;
    }

    if ((filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) &&
        (filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID)) {
        cur_list_item = &mac_item_p->vid_port_fltr_entry;
        parent_offset = offsetof(fdb_uc_mac_entry_t, vid_port_fltr_entry );
    }
    else if (filter->filter_by_log_port == FDB_KEY_FILTER_FIELD_VALID) {
        cur_list_item = &mac_item_p->port_fltr_entry;
        parent_offset = offsetof(fdb_uc_mac_entry_t, port_fltr_entry);
    }
    else if (filter->filter_by_vid == FDB_KEY_FILTER_FIELD_VALID) {
        cur_list_item = &mac_item_p->vid_fltr_entry;
        parent_offset = offsetof(fdb_uc_mac_entry_t, vid_fltr_entry);
    }
    else {
        err = -EINVAL;
        CL_LOG(CL_LOG_ERR, " invalid filter. err = %d\n", err);
        goto bail;
    }

    if (!cur_list_item) {
        err = -EPERM;
        CL_LOG(CL_LOG_ERR, " err = %d\n", err);
        goto bail;
    }
    next_list_item = cl_qlist_next(cur_list_item);
    if (next_list_item == list_cookie) {
        *mac_item_pp = NULL;
    }
    else {
        *mac_item_pp =
            (fdb_uc_mac_entry_t*) ((uint8_t*)next_list_item - parent_offset);
    }
bail:
    return err;
}


/*  Debug functions*/

/**
 * This function returns number of mac entries  in the specific vlan list
 *
 * @param[in]  vid - vlan filter value
 * @@param[out] count number of free elements in the pool.
 * @return count of mac entries  in the specific vlan list.
 *
 */
int
fdb_uc_db_filter_get_number_of_vid_entries(  unsigned short vid,
                                             uint32_t *count )
/* TODO */
{
    int err = 0;

    CHECK_FLTRS_INIT_DONE
    *count = 0;

    if (vid >= MAX_VLAN_ENTRIES) {
        return 0;
    }
    *count = fdb_vlan_list[vid].count;
bail:
    return err;
}


/**
 * This function returns number of mac entries  in the specific port list
 *
 * @param[in]   port - port filter value
 * @@param[out] count number of free elements in the pool.
 * @return count of mac entries  in the specific port list.
 */
int
fdb_uc_db_filter_get_number_of_port_entries(unsigned long port,
                                            uint32_t *count)
{
    cl_map_item_t   *map_item_p = NULL;
    struct fdb_fltr_entry  *fltr_item_p = NULL;
    uint64_t map_key = BUILD_PORT_KEY(port);
    int err = 0;

    CHECK_FLTRS_INIT_DONE;
    *count = 0;

    if (CL_QMAP_KEY_EXISTS(&fbd_filter_map, map_key, map_item_p)) {
        fltr_item_p = CL_QMAP_PARENT_STRUCT(struct fdb_fltr_entry);
        {
            cl_list_item_t *end = &fltr_item_p->head.end;
            cl_list_item_t *itor = cl_qlist_head(&fltr_item_p->head);
            while (itor != end) {
                itor = cl_qlist_next(itor);
                *count += 1;
            }
        }
    }
bail:
    return err;
}

/**
 * This function returns number of mac entries  in the specific port+vlan list
 *
 * @param[in]   port - port filter value
 * @param[in]   vlan - vlan filter value
 * @@param[out] count number of free elements in the pool.
 * @return count of mac entries  in the specific port+vlan list.
 */
int
fdb_uc_db_filter_get_number_of_port_vid_entries(unsigned long port,
                                                unsigned short vid,
                                                uint32_t *count)
{
    cl_map_item_t   *map_item_p = NULL;
    struct fdb_fltr_entry  *fltr_item_p = NULL;
    uint64_t map_key = BUILD_PORT_VID_KEY(port, vid );

    int err = 0;

    CHECK_FLTRS_INIT_DONE
    *count = 0;
    if (CL_QMAP_KEY_EXISTS(&fbd_filter_map, map_key, map_item_p)) {
        fltr_item_p = CL_QMAP_PARENT_STRUCT(struct fdb_fltr_entry);
        {
            cl_list_item_t * end = &fltr_item_p->head.end;
            cl_list_item_t * itor = cl_qlist_head(&fltr_item_p->head);
            while (itor != end) {
                itor = cl_qlist_next(itor);
                *count += 1;
            }
        }
    }
bail:
    return err;
}


/**
 * This function returns number of free elements in the pool used for qmap allocations
 *
 * @@param[out] count number of free elements in the pool.
 * @return error in case filters DB not inited
 *
 */
int
fdb_uc_db_filter_get_free_pool_count(uint32_t *count)
{
    int err = 0;
    *count = 0;
    CHECK_FLTRS_INIT_DONE

    if (!count) {
        err = -EPERM;
        CL_LOG(CL_LOG_ERR, " null pointer = %d\n", err);
        goto bail;
    }

    *count = cl_pool_count(&fdb_fltr_pool);
bail:
    return err;
}


