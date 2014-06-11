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

#ifndef __CTRL_LEARN_DEFS_H__
#define __CTRL_LEARN_DEFS_H__

/************************************************
 *  Macro definitions
 ***********************************************/

/**
 * Convert ether_addr structure to unsigned 64-bit value
 */
#define MAC_TO_U64(mac_addr) (((uint64_t)(mac_addr).ether_addr_octet[0] << \
        40) | \
                              ((uint64_t)(mac_addr).ether_addr_octet[1] << \
        32) | \
                              ((uint64_t)(mac_addr).ether_addr_octet[2] << \
        24) | \
                              ((uint64_t)(mac_addr).ether_addr_octet[3] << \
        16) | \
                              ((uint64_t)(mac_addr).ether_addr_octet[4] << 8) | \
                              ((uint64_t)(mac_addr).ether_addr_octet[5]))

#define MAC_EQUAL(mac1, mac2)                        \
    (MAC_TO_U64(mac1) == MAC_TO_U64(mac2))

/************************************************
 *  Type definitions
 ***********************************************/


/************************************************
 *  Macro definitions
 ***********************************************/

typedef uint64_t cl_key_t;

#define CL_LOG SX_LOG

#define CL_LOG_ERR  SX_LOG_ERROR
#define CL_LOG_WARN SX_LOG_WARNING
#define CL_LOG_NOTICE SX_LOG_NOTICE
#define CL_LOG_DEBUG SX_LOG_DEBUG


#define GET_KEY(item)       cl_qmap_key(&((item).map_item))
#define GET_KEY_P(map_item_p)   cl_qmap_key(&((map_item_p)->map_item))

#define CL_QMAP_KEY_EXISTS(map_p, key, map_item_p)     \
    ((map_item_p = \
          cl_qmap_get((map_p), (cl_key_t)(key))) != (cl_qmap_end(map_p)))

#define CL_QMAP_NEXT_KEY_EXISTS(map_p, key, map_item_p)   \
    ((map_item_p = \
          cl_qmap_get_next((map_p), (cl_key_t)(key))) != (cl_qmap_end(map_p)))

#define CL_QMAP_KEY_NOT_EXISTS(map_p, key, map_item_p) \
    ((map_item_p = \
          cl_qmap_get((map_p), (cl_key_t)(key))) == (cl_qmap_end(map_p)))

#define CL_QPOOL_GET(pool_item_p, pool_p)        \
    (NULL != (pool_item_p = cl_qpool_get(pool_p)))

#define CL_QPOOL_PUT(pool_item_p, pool_p)        \
    (cl_qpool_put(pool_p, pool_item_p))

#define CL_QMAP_KEY_REMOVE(map_p, key, map_item_p)     \
    ((map_item_p = \
          cl_qmap_remove(map_p, (cl_key_t)(key))) != (cl_qmap_end(map_p)))

#define CL_QLIST_PARENT_STRUCT(parent_type)     \
    PARENT_STRUCT(list_item_p, parent_type, list_item)

#define CL_QMAP_PARENT_STRUCT(parent_type)      \
    PARENT_STRUCT(map_item_p, parent_type, map_item)

#define CL_QPOOL_PARENT_STRUCT(parent_type)     \
    PARENT_STRUCT(pool_item_p, parent_type, pool_item)

#define CL_QMAP_END(map_item_p, map_p)       \
    ((map_item_p) == cl_qmap_end(map_p))

#define CL_QMAP_HEAD(map_item_p, map_p)          \
    ((map_item_p = cl_qmap_head(map_p)) != cl_qmap_end(map_p))

#define CL_QMAP_NEXT_KEY(map_item_p, map_p)      \
    (((map_item_p = cl_qmap_tail(map_p)) == \
      cl_qmap_end(map_p)) ? 0 : cl_qmap_key(map_item_p) + 1)

#define CL_QMAP_FOREACH(map_item_p, map_p)       \
    for (map_item_p = cl_qmap_head(map_p); map_item_p != cl_qmap_end(map_p); \
         map_item_p = cl_qmap_next(map_item_p))

#define CL_QMAP_COND_FOREACH(map_item_p, map_p, cond)  \
    for (map_item_p = cl_qmap_head(map_p); \
         (cond) && (map_item_p != cl_qmap_end(map_p)); \
         map_item_p = cl_qmap_next(map_item_p))

#define CL_QLIST_COUNT(list_p)              \
    (list_p != NULL ? cl_qlist_count(list_p) : 0)

#define CL_QLIST_POP(list_p)            \
    (list_p != NULL ? cl_qlist_remove_head(list_p) : NULL)

#define CL_QLIST_PUSH(list_p, list_item_p)      \
    cl_qlist_insert_head(list_p, list_item_p)


/************************************************
 *  MEM Macro definitions
 ***********************************************/

#ifndef MIN
#define MIN(x, y)    ((x) < (y) ? (x) : (y))
#endif

#define MEM_CLR(src)            (memset(&(src), 0, sizeof(src)))
#define MEM_CLR_P(src_p)            (memset((src_p), 0, sizeof(*(src_p))))
#define MEM_CLR_BUF(buf_p, size)         (memset((buf_p), 0, size))
#define MEM_CLR_TYPE(dst, type)          (memset((dst), 0, sizeof(type)))
#define MEM_CLR_ARRAY(dst, len, \
                      type)     (memset((dst), 0, (len) * sizeof(type)))

#define MEM_SET(src, val)            (memset(&(src), (val), sizeof(src)))
#define MEM_SET_P(src_p, val)        (memset((src_p), (val), sizeof(*(src_p))))
#define MEM_SET_BUF(buf, val, size)       (memset((buf), (val), (size)))
#define MEM_SET_TYPE(dst, val, type)      (memset((dst), (val), sizeof(type)))
#define MEM_SET_ARRAY(dst, val, len, \
                      type)      (memset((dst), (val), (len) * sizeof(type)))

#define MEM_CPY(dst, \
                src)            (memcpy(&(dst), &(src), \
                                        MIN(sizeof(src), sizeof(dst))))
#define MEM_CPY_P(dst_p, \
                  src_p)          (memcpy((dst_p), (src_p), \
                                          MIN(sizeof(*(src_p)), \
                                              sizeof(*(dst_p)))))
#define MEM_CPY_BUF(dst_buf, src_buf, \
                    size)    (memcpy((dst_buf), (src_buf), (size)))
#define MEM_CPY_TYPE(dst, src, type)      (memcpy((dst), (src), sizeof(type)))
#define MEM_CPY_ARRAY(dst, src, len, \
                      type)      (memcpy((dst), (src), (len) * sizeof(type)))

#define TMP_SWAP(dst, src, tmp)           do {    \
        tmp = dst;                          \
        dst = src;                          \
        src = tmp;                          \
} while (0)

#define MEM_SWAP_P(dst_p, src_p, tmp_p)       do {    \
        *(tmp_p) = *(dst_p);                    \
        *(dst_p) = *(src_p);                    \
        *(src_p) = *(tmp_p);                    \
} while (0)

#define MEM_SWAP_BUF(dst_buf, src_buf, tmp_buf, size)   do {    \
        MEM_CPY_BUF(tmp_buf, dst_buf, size);          \
        MEM_CPY_BUF(dst_buf, src_buf, size);          \
        MEM_CPY_BUF(src_buf, tmp_buf, size);          \
} while (0)

#define MIN_FDB_ENTRIES      49152
#define MAX_FDB_ENTRIES      49152

#define MAX_PORT_ENTRIES     200
#define MAX_FILTER_ENTRIES   (MAX_FDB_ENTRIES + MAX_PORT_ENTRIES)
#define MIN_FILTER_ENTRIES   MAX_FILTER_ENTRIES

#define MAX_VLAN_ENTRIES     4096


#endif /* __CTRL_LEARN_DEFS_H__ */
