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
 *	Declaration of communication channel.
 *	The communication channel enables communication between client
 *	and server.
 */

#ifndef _CL_COMM_CHNL_H_
#define _CL_COMM_CHNL_H_

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
#include <complib/cl_commchnl_osd.h>
/****h* Component Library/Communication Channel
* NAME
*	Communication Channel
*
* DESCRIPTION
*	The Communication Channel provides a self-contained object for client/
*	server communication.
*
*	The communication channel functions operates on a cl_commchnl_t
*	structure which should be treated as opaque and should be manipulated
*	only through the provided functions.
*
*	A client side first needs to initialize a communication channel. Upon
*	successful initialization the communication channel is ready for send
*	and receive operations.
*
*	A server side first needs to initialize a communication channel. Upon
*	successful initialization the communication channel is ready to accept
*	clients. After a client is accepted at the server side, a new instance
*	of the communication channel is created. This enables direct
*	communication with the client, and the server remains free to accept
*	new clients.
*
* SEE ALSO
*	Structures:
*		cl_commchnl_t
*
*	Initialization/Destruction:
*		cl_commchnl_init, cl_commchnl_destroy
*
*	Manipulation:
*		cl_commchnl_accept, cl_commchnl_send, cl_commchnl_recv,
*		cl_commchnl_get_fd
*
*	Attributes:
*		cl_is_commchnl_inited, cl_commchnl_side
*********/

/****f* Component Library: Communication Channel/cl_is_commchnl_inited
* NAME
*	cl_is_commchnl_inited
*
* DESCRIPTION
*	The cl_is_commchnl_inited function returns whether a communication channel was
*	successfully initialized.
*
* SYNOPSIS
*/
static inline boolean_t cl_is_commchnl_inited(IN const cl_commchnl_t * const p_commchnl)
{
	/* CL_ASSERT that a non-null pointer is provided. */
	CL_ASSERT(p_commchnl);
	return (p_commchnl->state == CL_INITIALIZED);
}

/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure whose initialization state
*		to check.
*
* RETURN VALUES
*	TRUE if the communication channel was initialized successfully.
*
*	FALSE otherwise.
*
* NOTES
*	Allows checking the state of a communication channel to determine if invoking
*	member functions is appropriate.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_init
*********/

/****f* Component Library: Communication Channel/cl_commchnl_side
* NAME
*	cl_commchnl_side
*
* DESCRIPTION
*	The cl_commchnl_side function returns the communication channel side.
*
* SYNOPSIS
*/
static inline cl_commchnl_side_t cl_commchnl_side(IN const cl_commchnl_t * const p_commchnl)
{
	/* CL_ASSERT that a non-null pointer is provided. */
	CL_ASSERT(p_commchnl);
	return (p_commchnl->side);
}

/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure whose side to check.
*
* RETURN VALUES
*	CL_COMMCHNL_SIDE_CLIENT if the communication channel was initialized as client.
*
*	CL_COMMCHNL_SIDE_SERVER if the communication channel was initialized as server.
*
* NOTES
*	Allows checking the side of a communication channel to determine if invoking
*	member functions is appropriate.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_init
*********/

/****f* Component Library: Communication Channel/cl_commchnl_init
* NAME
*	cl_commchnl_init
*
* DESCRIPTION
*	The cl_commchnl_init function initializes a communication channel for use.
*
* SYNOPSIS
*/
cl_status_t
cl_commchnl_init(IN cl_commchnl_t * const p_commchnl,
		 IN const char * const address,
		 IN cl_commchnl_side_t side);
/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure to initialize.
*
*	address
*		[in] Communication Channel address. Both sides need to
*		open the communication channel with the same address.
*
*	side
*		[in] Communication Channel side.
*
* NOTES
*	After cl_commchnl_init function called the client side ready to
*	send data and the server side ready to accept new clients.
*
* RETURN VALUES
*	CL_SUCCESS if the communication channel was initialized successfully.
*
*	CL_ERROR if an undefined error occured.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_destroy, cl_commchnl_accept
*	cl_commchnl_send, cl_commchnl_recv, cl_commchnl_get_fd
*********/

/****f* Component Library: Communication Channel/cl_commchnl_destroy
* NAME
*	cl_commchnl_destroy
*
* DESCRIPTION
*	The cl_commchnl_destroy function destroys a communication channel.
*
* SYNOPSIS
*/
void
cl_commchnl_destroy(IN cl_commchnl_t * const p_commchnl);
/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	The communication channel is closed and all memory allocated
*	is freed. Further operations on the communication channel or the
*	returned file descriptor should not be attempted after
*	cl_commchnl_destroy is invoked.
*
*	This function should only be called after a call to cl_commchnl_init.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_init
*********/

/****f* Component Library: Communication Channel/cl_commchnl_accept
* NAME
*	cl_commchnl_accept
*
* DESCRIPTION
*	The cl_commchnl_accept function accept new clients of the communication
*	channel.
*
* SYNOPSIS
*/
cl_status_t
cl_commchnl_accept(IN cl_commchnl_t * const p_commchnl,
		   OUT cl_commchnl_t * const p_commchnl_client);
/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure from which to accept
*		connections.
*
*	p_commchnl_client
*		[out] Pointer to a cl_commchnl_t structure of the new client.
*
* NOTES
*	cl_commchnl_accept function is blocking and can be called only from
*	server side of the communication channel.
*
* RETURN VALUES
*	CL_SUCCESS if the accept operation was successful.
*
*	CL_ERROR if an undefined error occured.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_send, cl_commchnl_recv,
*	cl_commchnl_get_fd
*********/

/****f* Component Library: Communication Channel/cl_commchnl_send
* NAME
*	cl_commchnl_send
*
* DESCRIPTION
*	The cl_commchnl_send function sends data over the communication channel.
*
* SYNOPSIS
*/
cl_status_t
cl_commchnl_send(IN cl_commchnl_t * const p_commchnl,
		 IN const uint8_t * const p_buffer,
		 IN OUT uint32_t * buffer_size);
/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure.
*
*	p_buffer
*		[in] Pointer to a buffer from which to send the data.
*
*	buffer_size
*		[in] buffer's size in bytes.
*		[out] Number of bytes sent.
*
* NOTES
*	cl_commchnl_send function is blocking and can be called only from
*	client side of the communication channel.
*
* RETURN VALUES
*	CL_SUCCESS if the send operation was successful.
*
*	CL_ERROR if an undefined error occured.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_accept, cl_commchnl_recv,
*	cl_commchnl_get_fd
*********/

/****f* Component Library: Communication Channel/cl_commchnl_recv
* NAME
*	cl_commchnl_recv
*
* DESCRIPTION
*	The cl_commchnl_recv function receive data over the communication channel.
*
* SYNOPSIS
*/
cl_status_t
cl_commchnl_recv(IN cl_commchnl_t * const p_commchnl,
		 IN boolean_t recv_all,
		 IN uint8_t * const p_buffer,
		 IN OUT uint32_t * buffer_size);
/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure.
*
*	recv_all
*		[in] Indicates whether the receive operation waits for all data.
*
*	p_buffer
*		[in] Pointer to a buffer into which receive the data.
*
*	buffer_size
*		[in] buffer's size in bytes.
*		[out] Number of bytes received.
*
* NOTES
*	cl_commchnl_recv function is blocking and can be called only from
*	client side of the communication channel.
*
* RETURN VALUES
*	CL_SUCCESS if the send operation was successful.
*
*	CL_DISCONNECT if the communication channel closed.
*
*	CL_ERROR if an undefined error occured.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_accept, cl_commchnl_send,
*	cl_commchnl_get_fd
*********/

/****f* Component Library: Communication Channel/cl_commchnl_get_fd
* NAME
*	cl_commchnl_get_fd
*
* DESCRIPTION
*	The cl_commchnl_get_fd function returns file descriptor in order
*	to allow select/poll operations.
*
* SYNOPSIS
*/
int
cl_commchnl_get_fd(IN cl_commchnl_t * const p_commchnl);
/*
* PARAMETERS
*	p_commchnl
*		[in] Pointer to a cl_commchnl_t structure.
*
* RETURN VALUES
*	> 0 if the operation was successful.
*
*	0 if an undefined error occured.
*
* NOTES
*	The returned file descriptor should be used only to
*	signal activity at the communication channel.
*
* SEE ALSO
*	Communication Channel, cl_commchnl_accept, cl_commchnl_send,
*	cl_commchnl_recv
*********/

END_C_DECLS
#endif				/* _CL_COMM_CHNL_H_ */
