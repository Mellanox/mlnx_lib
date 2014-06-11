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

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <complib/cl_commchnl.h>
#include <signal.h>
#include <sys/types.h>


cl_status_t
cl_commchnl_init(IN cl_commchnl_t * const p_commchnl,
		 IN const char * const address,
		 IN cl_commchnl_side_t side)
{
	int len, ret;
	struct sockaddr_un sun;

	CL_ASSERT(p_commchnl);
	CL_ASSERT(address);
	CL_ASSERT(side == CL_COMMCHNL_SIDE_CLIENT || side == CL_COMMCHNL_SIDE_SERVER);

	p_commchnl->state = CL_UNINITIALIZED;
	p_commchnl->side = side;

	p_commchnl->socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (p_commchnl->socket == -1) {
		return (CL_ERROR);
	}

	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, address, sizeof(sun.sun_path));
	sun.sun_path[sizeof(sun.sun_path) - 1] = '\0';
	len = strlen(sun.sun_path) + sizeof(sun.sun_family);

	if (p_commchnl->side == CL_COMMCHNL_SIDE_SERVER) {
		unlink(sun.sun_path);

		ret = bind(p_commchnl->socket, (struct sockaddr *)&sun, len);
		if (ret == -1) {
			return (CL_ERROR);
		}

		ret = listen(p_commchnl->socket, 10);
		if (ret == -1) {
			return (CL_ERROR);
		}
	} else {
		ret = connect(p_commchnl->socket, (struct sockaddr *)&sun, len);
		if (ret == -1) {
			return (CL_ERROR);
		}
	}

    p_commchnl->pid = getpid();
	p_commchnl->state = CL_INITIALIZED;
	return (CL_SUCCESS);
}

void
cl_commchnl_destroy(IN cl_commchnl_t * const p_commchnl)
{
	CL_ASSERT(p_commchnl);
	CL_ASSERT(cl_is_state_valid(p_commchnl->state));

	if (p_commchnl->state == CL_INITIALIZED) {
		p_commchnl->state = CL_UNINITIALIZED;
		close(p_commchnl->socket);
	}
	p_commchnl->state = CL_UNINITIALIZED;
}

cl_status_t
cl_commchnl_accept(IN cl_commchnl_t * const p_commchnl,
		   OUT cl_commchnl_t * const p_commchnl_client)
{
	unsigned len;
	struct sockaddr_un sun;

	CL_ASSERT(p_commchnl);
	CL_ASSERT(p_commchnl->state == CL_INITIALIZED);
	CL_ASSERT(p_commchnl->side == CL_COMMCHNL_SIDE_SERVER);
	CL_ASSERT(p_commchnl_client);

	len = sizeof(struct sockaddr_un);

	p_commchnl_client->socket = accept(p_commchnl->socket, (struct sockaddr *)&sun, &len);
	if (p_commchnl_client->socket == -1) {
		return (CL_ERROR);
	}

	p_commchnl_client->side = CL_COMMCHNL_SIDE_CLIENT;
	p_commchnl_client->state = CL_INITIALIZED;
	return (CL_SUCCESS);
}

cl_status_t
cl_commchnl_send(IN cl_commchnl_t * const p_commchnl,
		 IN const uint8_t * const p_buffer,
		 IN uint32_t * buffer_size)
{
	uint32_t bytes_sent = 0;
	int bytes_left = *buffer_size;
	int ret = 0;

	CL_ASSERT(p_commchnl);
	CL_ASSERT(p_commchnl->state == CL_INITIALIZED);
	CL_ASSERT(p_commchnl->side == CL_COMMCHNL_SIDE_CLIENT);
	CL_ASSERT(p_buffer);

	while (bytes_sent < *buffer_size) {
		ret = send(p_commchnl->socket, p_buffer + bytes_sent, bytes_left, MSG_NOSIGNAL);
		if (ret <= 0) {
			break;
		}

		bytes_sent += ret;
		bytes_left -= ret;
	}

	*buffer_size = bytes_sent;
	return (bytes_left ? CL_ERROR : CL_SUCCESS);
}

cl_status_t
cl_commchnl_recv(IN cl_commchnl_t * const p_commchnl,
		 IN boolean_t recv_all,
		 IN uint8_t * const p_buffer,
		 IN OUT uint32_t * buffer_size)
{
	uint32_t bytes_recv = 0;
	int bytes_left = *buffer_size;
	int ret = 0;

	CL_ASSERT(p_commchnl);
	CL_ASSERT(p_commchnl->state == CL_INITIALIZED);
	CL_ASSERT(p_commchnl->side == CL_COMMCHNL_SIDE_CLIENT);
	CL_ASSERT(p_buffer);

	ret = recv(p_commchnl->socket, p_buffer, *buffer_size, 0);
	if (ret == -1) {
		*buffer_size = -1;
		return (CL_ERROR);
	}
	if (ret == 0) {
		*buffer_size = 0;
		return (CL_DISCONNECT);
	}
	bytes_recv += ret;
	bytes_left -= ret;

	while (recv_all && (bytes_recv < *buffer_size)) {
		ret = recv(p_commchnl->socket, p_buffer + bytes_recv, bytes_left, 0);
		if (ret <= 0) {
			break;
		}

		bytes_recv += ret;
		bytes_left -= ret;
	}

	if (ret == -1) {
		*buffer_size = -1;
		return (CL_ERROR);
	}
	if (ret == 0) {
		*buffer_size = 0;
		return (CL_DISCONNECT);
	}
	*buffer_size = bytes_recv;
	return (CL_SUCCESS);
}

int
cl_commchnl_get_fd(IN cl_commchnl_t * const p_commchnl)
{
	CL_ASSERT(p_commchnl);
	CL_ASSERT(cl_is_state_valid(p_commchnl->state));

	return (p_commchnl->state == CL_INITIALIZED ? p_commchnl->socket : 0);
}
