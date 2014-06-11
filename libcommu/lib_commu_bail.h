/*
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2014. ALL RIGHTS RESERVED.
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

#ifndef LIB_COMMU_BAIL_H_
#define LIB_COMMU_BAIL_H_



/************************************************
 *  Defines
 ***********************************************/

#define lib_commu_bail_force(error)           \
    do {                            \
        err = (error);              \
        goto bail;                  \
    } while (0)


#define lib_commu_bail_error(error)           \
    do {                            \
        if (error) {                \
            lib_commu_bail_force(error);      \
        }                           \
    } while (0)

/* use within bail */
#define lib_commu_return_from_bail(error)     \
    do {                            \
        err = (error);              \
        return err;                 \
    } while (0)

#define lib_commu_bail_null(ptr)              \
    do {                            \
        if (ptr == 0) {             \
            lib_commu_bail_force(EINVAL);     \
        }                           \
    } while (0)

#endif /* LIB_COMMU_BAIL_H_ */
