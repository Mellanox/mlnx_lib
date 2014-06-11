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

/*
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2013.�� ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of Mellanox Technologies, Ltd.
 * (the "Company") and all right, title, and interest in and to the software product,
 * including all associated intellectual property rights, are and shall
 * remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 */

#include <complib/sx_log.h>
#include <complib/cl_qmap.h>
#include <complib/cl_pool.h>
#include <stdarg.h>
#include <stdio.h>
/*#include "mlag_log.h"*/
#include "lib_ctrl_learn_defs.h"
#include "lib_ctrl_learn_uc_db.h"
#include "lib_ctrl_learn_filters.h"

#include <sys/time.h>
#include  "/usr/include/asm-generic/errno-base.h"

void trace(unsigned int level, const char * module, const char *fmt, ...);

int test_filters_getters(void);

/*#define __MODULE__ CTRL_LEARN_DBG
   static mlag_verbosity_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_NOTICE;
 */

void
trace( unsigned int level, const char * module, const char *fmt, ...)
{
    /* create formatted str */

    if (level != CL_LOG_WARN) {
        int len;
        va_list ap;
        char trace_buffer[1000];
        va_start(ap, fmt);
        len = vsprintf(trace_buffer, fmt, ap);
        sx_log(level, module, trace_buffer);
        va_end(ap);
    }
}


#define MAX_MAC_ENTRIES 49152

int
main()
{
    fdb_uc_mac_entry_t mac_entry;
    fdb_uc_mac_entry_t * mac_entry_p;
    uint64_t mac_key = 0;
    int err = 0;
    uint32_t                * mac = NULL;
    unsigned int i, interval, db_size;
    struct timeval tv_start, tv_stop;

    /*err = fdb_uc_db_init(sx_log) ;*/
    err = fdb_uc_db_init(trace);
    memset( &mac_entry.mac_params.mac_addr, 0, 6);
    mac = (uint32_t *)&mac_entry.mac_params.mac_addr;

    mac_entry.mac_params.vid = 30;
    mac_entry.mac_params.log_port = 30000;


    gettimeofday(&tv_start, NULL);
    /*1. Add MAX entries to the MAP*/
    for (i = 0; i < MAX_MAC_ENTRIES; i++) {
        *mac = *mac + 1; /* increment mac*/
        if (i == 40) { /* simulate modify of mac*/
            *mac = *mac - 1;
            mac_entry.mac_params.log_port = 20000;
        }

        err = fdb_uc_db_add_record(   &mac_entry);
        if (err) {
            printf(" set err %d, i= %d\n", err, i);
            goto bail;
        }
        mac_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(mac_entry.mac_params.mac_addr,
                                                 30);
        err = fdb_uc_db_get_record_by_key(  mac_key, &mac_entry_p);
        if (err) {
            printf(" get err %d, i= %d\n", err, i);
            goto bail;
        }
        /*printf(" err %d, vlan %d\n",err, mac_entry_p->mac_params.vid);*/
    }

    /*2. GET MAX entries from the map with get "next"  method*/

    i = 0;
    err = fdb_uc_db_get_first_record(  &mac_entry_p);
    if (err) {
        printf(" get first err %d, i= %d\n", err, i);
        goto bail;
    }

    while (mac_entry_p != NULL) {
        err = fdb_uc_db_next_record(  mac_entry_p, &mac_entry_p);
        if (err) {
            printf(" get next err %d, i= %d\n", err, i);
            goto bail;
        }
        i++;
    }

    test_filters_getters();

/*
    3. Delete all Entries using flush  vid interface
    err=  fdb_uc_db_flush_all_by_vid( &fdb_map, 20);
    if(err)
    {
       printf(" flush by vid err %d \n",err);
       goto bail;
    }
    err = fdb_uc_db_get_size(&fdb_map, &db_size);
    printf(" flush by vid err %d   size = %d \n",err, db_size);

    err=  fdb_uc_db_flush_all( &fdb_map);
    if(err)
    {
       printf(" flush all err %d \n",err);
         goto bail;
    }
    err = fdb_uc_db_get_size(&fdb_map, &db_size);
    printf(" flush all err %d   size = %d \n",err, db_size);

 */

    /*4. Delete all Entries one by one*/
    memset( &mac_entry.mac_params.mac_addr, 0, 6);
    for (i = 0; i < MAX_MAC_ENTRIES; i++) {
        *mac = *mac + 1;

        mac_key = FDB_UC_CONVERT_MAC_VLAN_TO_KEY(mac_entry.mac_params.mac_addr,
                                                 30);
        err = fdb_uc_db_get_record_by_key(  mac_key, &mac_entry_p);
        if (err) {
            printf(" delete cycle get err %d, i= %d\n", err, i);
            goto bail;
        }
        err = fdb_uc_db_delete_record(   mac_entry_p);
        if (err) {
            printf(" delete err %d, i= %d\n", err, i);
            goto bail;
        }
    }

bail:
    gettimeofday(&tv_stop, NULL);
    interval = tv_stop.tv_sec * 1000 + tv_stop.tv_usec / 1000 -
               (tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000);
    err = fdb_uc_db_get_size(&db_size);

    uint32_t c1, c2, c3;

    fdb_uc_db_filter_get_number_of_vid_entries(30, &c1);
    fdb_uc_db_filter_get_number_of_port_entries(30000, &c2);
    fdb_uc_db_filter_get_number_of_port_vid_entries(30000, 30, &c3);

    printf(
        "\ncompleted, i = %d, interval %d   size = %d,Filters : num  vlans = %d, num ports = %d  num port_vlan = %d  \n",
        i, interval, db_size, c1, c2, c3);

    uint32_t fdb_count, filter_count;

    fdb_uc_db_get_free_pool_count(&fdb_count);
    fdb_uc_db_filter_get_free_pool_count(&filter_count);
    printf("Num free items in pools : in db pool %d, in filter pool %d \n",
           fdb_count, filter_count  );



    /* TEST deinit*/
    err = fdb_uc_db_deinit();
    if (err) {
        printf("Error destroy FBD\n");
    }

    err = fdb_uc_db_init(trace);
    fdb_uc_db_get_free_pool_count(&fdb_count);
    fdb_uc_db_filter_get_free_pool_count(&filter_count);
    printf(
        "Num free items in pools after deinit and init  : in db pool %d, in filter pool %d \n",
        fdb_count, filter_count  );



    return err;
}



int
test_filters_getters( void )
{
    int err = 0;
    struct fdb_uc_key_filter filter;
    fdb_uc_mac_entry_t   * mac_item_p, * cur_mac_entry;
    void                    * list_cookie = NULL;
    int count = 0;

    filter.filter_by_vid = FDB_KEY_FILTER_FIELD_VALID;
    filter.vid = 30;
    filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_NOT_VALID;


    err = fdb_uc_db_get_first_record_by_filter( &filter,  &list_cookie,
                                                &mac_item_p );

    cur_mac_entry = mac_item_p;


    while (mac_item_p != NULL) {
        count++;
        err = fdb_uc_db_get_next_record_by_filter(  &filter, list_cookie,
                                                    cur_mac_entry,
                                                    &mac_item_p);
        if (err) {
            break;
        }
        if (mac_item_p) {
            cur_mac_entry = mac_item_p;
        }
    }
    printf("test_filters_getters  vlan count = %d \n", count);


    /*filter.vid = 20;*/
    filter.log_port = 30000;
    filter.filter_by_log_port = FDB_KEY_FILTER_FIELD_VALID;
    /*filter.filter_by_vid      = FDB_KEY_FILTER_FIELD_NOT_VALID;*/

    err = fdb_uc_db_get_first_record_by_filter( &filter,  &list_cookie,
                                                &mac_item_p );

    if (err) {
        return 0;
    }
    cur_mac_entry = mac_item_p;
    count = 0;
    while (mac_item_p != NULL) {
        count++;
        err = fdb_uc_db_get_next_record_by_filter(  &filter, list_cookie,
                                                    cur_mac_entry,
                                                    &mac_item_p);
        if (err) {
            break;
        }
        cur_mac_entry = mac_item_p;
    }
    printf("test_filters_getters  port count = %d \n", count);



    return err;
}





