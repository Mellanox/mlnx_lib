/*
 * Copyright (c) 2010 Mellanox Technologies LTD. All rights reserved.
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
 *	Implementation of communication channel object.
 */

#ifndef _CL_COMMCHNL_OSD_H_
#define _CL_COMMCHNL_OSD_H_

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
#include <complib/cl_types.h>
#include <sys/socket.h>

/****d* Component Library: Communication Channel/cl_commchnl_side_t
* NAME
*	cl_commchnl_side_t
*
* DESCRIPTION
*	The cl_commchnl_side_t enumerated type is used to note the side
*	of the communication channel.
*
* SYNOPSIS
*/
typedef enum _cl_commchnl_side {
	CL_COMMCHNL_SIDE_CLIENT = 0,
	CL_COMMCHNL_SIDE_SERVER,

	CL_COMMCHNL_SIDE_COUNT		/* should be the last value */
} cl_commchnl_side_t;
/*
* SEE ALSO
*	Communication Channel
*********/

/****d* Component Library: Communication Channel/cl_commchnl_t
* NAME
*	cl_commchnl_t
*
* DESCRIPTION
*	Communication Channel structure.
*
*	The cl_commchnl_t structure should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct _cl_commchnl {
	int socket;
	cl_commchnl_side_t side;
	cl_state_t state;
	pid_t pid;
} cl_commchnl_t;
/*
* FIELDS
*	socket
*		File descriptor that holds unix domain socket.
*
*	side
*		Side of the communication channel.
*
*	state
*		State of the communication channel.
*
* SEE ALSO
*	Communication Channel
*********/

END_C_DECLS
#endif				/* _CL_COMMCHNL_OSD_H_ */
