/*
 * Copyright (c) 2004-2006 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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

#ifndef __SX_RPC_H__
#define __SX_RPC_H__

#include <complib/cl_types.h>
#include <complib/sx_log.h>
#include <sys/types.h>


/**
 * RPC handle.
 */
typedef uint64_t sx_rpc_handle_t;

/**
 * Invalid Handle Definition
 */
#define SX_RPC_INVALID_HANDLE   0


typedef enum sx_rpc_status {
	SX_RPC_STATUS_SUCCESS,
	SX_RPC_STATUS_ERROR,
	SX_RPC_STATUS_NO_RESOURCES,
	SX_RPC_STATUS_MEMORY_ERROR,
	SX_RPC_STATUS_CMD_INCOMPLETE,
	SX_RPC_STATUS_PARAM_NULL,
	SX_RPC_STATUS_INT_COMM_CLOSE,
	SX_RPC_STATUS_INVALID_HANDLE,

	SX_RPC_STATUS_MAX
} sx_rpc_status_t;


typedef enum sx_rpc_internal_command {
	SX_RPC_API_CMD_UNKNOWN_EVENT_COMMAND,
	SX_RPC_API_CMD_CLOSE_EVENT_SRC,

	SX_RPC_API_LAST_INTERNAL_COMMAND,
} sx_rpc_internal_command_t;



typedef enum sx_rpc_cmd_priority {
	SX_RPC_API_CMD_PRIO_LOW = 0,
	SX_RPC_API_CMD_PRIO_MEDIUM,
	SX_RPC_API_CMD_PRIO_HIGH,
} sx_rpc_cmd_priority_t;

/**
 *  callback function provided by the server for every command
 */
typedef int (*sx_rpc_command_fp_t)(uint8_t *rcv_msg_body,uint32_t rcv_len,uint8_t **snd_body,uint32_t *snd_len);

/**
 * sx_rpc_api_command data structure - using by server side registering.
 */
typedef struct sx_rpc_api_command {
	int 			cmd_id;		 /* enum representing command */
	char 			name[100];	 /* command name for logging purpose */
	sx_rpc_command_fp_t 	func; 		 /* func pointer */
	sx_rpc_cmd_priority_t 	priority;	 /* priority within thread */
} sx_rpc_api_command_t;

/**
 * sx_rpc_api_command_head_t - this data structure use for header command message
 */

typedef struct sx_rpc_api_command_head {
	uint32_t opcode;
	uint32_t version;
	uint32_t msg_size;
	uint32_t list_size;
	pid_t pid;
} sx_rpc_api_command_head_t;

/**
 *  sx_rpc_api_reply_head_t - this data structure use for header reply messages
 */
typedef struct sx_rpc_api_reply_head {
	uint32_t retcode;
	uint32_t version;
	uint32_t msg_size;
	uint32_t list_size;
} sx_rpc_api_reply_head_t;

void sx_rpc_register_log_function(sx_log_cb_t log_cb);

/**
 * This function signals the RPC server to stop/restart inserting API requests to the system.
 * Uses this API when you don't wont to respond to any requests
 *
 * @param[in] bool - TRUE: stop inserting API requests.
 * 					FALSE: restart inserting API requests.
 *
 * @return no return status
 */
void sx_rpc_change_main_exit_signal_state(boolean_t bool);


/**
 * server APIs - need to call sx_rpc_core_td_init and sx_rpc_api_rpc_command_db before sx_rpc_core_td_start_loop
 * the application support only one server per process
 */
sx_rpc_status_t
sx_rpc_core_td_init(char *commmchannel_name,
		    uint32_t max_message_size);

sx_rpc_status_t
sx_rpc_api_rpc_command_db_init(sx_rpc_api_command_t *psx_core_api_cmd_table,
			  uint32_t num_of_item);

sx_rpc_status_t
sx_rpc_api_rpc_command_db_deinit();

void sx_rpc_core_td_start_loop(void);


/**
 * client APIs - need to call sx_rpc_api_open and then sx_rpc_api_send_command_decoupled for send message to server
 */

/**
 *  This function open a channel to RPC operations.
 *
 * @param[out] handle - RPC handle
 * @param[in] commchannel_name - communication channel address
 *
 * @return SX_RPC_STATUS_SUCCESS if operation completes successfully
 * @return SX_RPC_STATUS_PARAM_NULL if any input parameter is invalid
 */
sx_rpc_status_t
sx_rpc_open(sx_rpc_handle_t *handle,
	    const char *commchannel_name);

/**
 *  This function sends a command to RPC server and waits for
 *  the reply.
 *
 * @param[in] handle - RPC handle.
 * @param[in] cmd_head - command head structure.
 * @param[in] cmd_body - command body buffer.
 * @param[out] reply_head - reply head buffer.
 * @param[out] reply_body - reply body buffer.
 * @param[in] reply_body_size - reply body size.
 *
 * @return sx_status_t:
 * @return SX_RPC_STATUS_SUCCESS - Operation completes successfully
 * @return SX_RPC_STATUS_INVALID_HANDLE - Input parameters error - Invalid handle
 */
sx_rpc_status_t
sx_rpc_send_command_decoupled(sx_rpc_handle_t handle,
			      sx_rpc_api_command_head_t *cmd_head,
			      uint8_t *cmd_body,
			      sx_rpc_api_reply_head_t *reply_head,
			      uint8_t *reply_body,
			      uint32_t reply_body_size);

/**
 *  This function closes channel to RPC operations.
 *
 * @param[in] handle - RPC handle.
 *
 * @return sx_status_t:
 * @return SX_RPC_STATUS_SUCCESS - Operation completes successfully
 * @return SX_RPC_STATUS_PARAM_NULL - Input parameters error - NULL handle
 * @return SX_RPC_STATUS_INVALID_HANDLE - Input parameters error - Invalid handle
 */
sx_rpc_status_t
sx_rpc_close(sx_rpc_handle_t *handle);


#endif /*__SX_RPC_H__*/
