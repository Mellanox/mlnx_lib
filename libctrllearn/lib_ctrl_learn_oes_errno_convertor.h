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

#ifndef LIB_CTRL_LEARN_OES_ERRNO_CONVERTOR_H_
#define LIB_CTRL_LEARN_OES_ERRNO_CONVERTOR_H_


#include "oes_status.h"
#include "errno.h"

static inline int
oes_status_to_errno(enum oes_status status)
{
    int err = 0;
    if (status > OES_STATUS_MAX) {
        err = -EPERM;
    }
    else {
        switch (status) {
        case OES_STATUS_SUCCESS:
            err = 0;
            break;
        case OES_STATUS_ERROR:
            err = -EPERM;
            break;
        case OES_STATUS_CMD_UNSUPPORTED:
            err = -EOPNOTSUPP;
            break;
        case OES_STATUS_PARAM_ERROR:
            err = -EINVAL;
            break;
        case OES_STATUS_PARAM_EXCEEDS_RANGE:
            err = -EINVAL;
            break;
        case OES_STATUS_NO_MEMORY:
            err = -EPERM;
            break;
        case OES_STATUS_OES_NOT_INITIALIZED:
            err = -EPERM;
            break;
        case OES_STATUS_ENTRY_NOT_FOUND:
            err = -ENOENT;
            break;
        case OES_STATUS_ENTRY_ALREADY_EXISTS:
            err = -EEXIST;
            break;
        case OES_STATUS_NO_RESOURCES:
            err = -EXFULL;
            break;
        default:
            err = -EPERM;
            break;
        }
    }

    return err;
}

#endif /* LIB_CTRL_LEARN_OES_ERRNO_CONVERTOR_H_ */
