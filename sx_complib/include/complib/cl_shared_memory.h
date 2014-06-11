/*
 * Copyright (c) 2002-2011 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
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
 *
 */

/*
 * Abstract:
 *	This file contains the shared memory.
 */

#ifndef _CL_SHARED_MEMORY_H_
#define _CL_SHARED_MEMORY_H_

#include <complib/cl_types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

static inline cl_status_t cl_shm_create(IN char * const path, OUT int *shmid)
{
	CL_ASSERT(path);
	CL_ASSERT(shmid);

	*shmid = shm_open(path, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (*shmid < 0)
		return CL_ERROR;

	return (CL_SUCCESS);
}

static inline cl_status_t cl_shm_open(IN char * const path, OUT int *shmid)
{
	CL_ASSERT(path);

	*shmid = shm_open(path, O_RDWR, 0666);
	if (*shmid < 0)
		return CL_ERROR;

	return (CL_SUCCESS);
}

static inline cl_status_t cl_shm_destroy(IN char * const path)
{
	int err;

	CL_ASSERT(path);

	err = shm_unlink(path);
	if (err == -1)
		return CL_ERROR;

	return (CL_SUCCESS);
}

END_C_DECLS
#endif				/* _CL_SHARED_MEMORY_H_ */
