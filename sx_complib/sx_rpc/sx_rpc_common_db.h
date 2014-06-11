#ifndef __SX_RPC_COMMON_DB_H__
#define __SX_RPC_COMMON_DB_H__

/*
 *                  - Mellanox Confidential and Proprietary -
 *
 *  Copyright (C) January 2013 , Mellanox Technologies Ltd.  ALL RIGHTS RESERVED.
 *
 *  Except as specifically permitted herein , no portion of the information ,
 *  including but not limited to object code and source code , may be reproduced ,
 *  modified , distributed , republished or otherwise exploited in any form or by
 *  any means for any purpose without the prior written permission of Mellanox
 *  Technologies Ltd. Use of software subject to the terms and conditions
 *  detailed in the file "LICENSE.txt".
 */

#include <complib/cl_qmap.h>
#include <complib/cl_math.h>
#include <complib/cl_passivelock.h>

#define SX_RPC_API_LOW_PRIO_MESSAGE_SIZE_LIMIT (128)
#define	SX_RPC_CORE_TD_WORKER_NUM				1
#define SX_RPC_API_INT_VERSION 					1


#define	SX_RPC_API_VERSION_MAX_LEN				20


typedef struct sx_rpc_comlib_db_data{
	cl_qmap_t 			cmd_map;
	cl_plock_t 			cmd_db_mutex;
} sx_rpc_comlib_db_data;


typedef enum {
	SX_RPC_CORE_CMD_DB_GET,
	SX_RPC_CORE_CMD_DB_SET,
}sx_rpc_cmd_db_op;


typedef enum sx_rpc_event_src_state {
	SX_RPC_EVENT_SRC_STATE_NOT_ACTIVE = 0 ,
	SX_RPC_EVENT_SRC_STATE_TERMINATING,
	SX_RPC_EVENT_SRC_STATE_ACTIVE,
	SX_RPC_EVENT_SRC_STATE_LAST
} sx_rpc_event_src_state_t;


typedef enum  {
	SX_RPC_CORE_LOW_PRIO_BUF =0,
    SX_RPC_CORE_MED_PRIO_BUF ,
    SX_RPC_CORE_HIGH_PRIO_BUF,

    SX_RPC_CORE_MIN_PRIO_BUF=SX_RPC_CORE_LOW_PRIO_BUF,
    SX_RPC_CORE_MAX_PRIO_BUF=SX_RPC_CORE_HIGH_PRIO_BUF,
    SX_RPC_CORE_MAX_NON_HIGH_PRIO_BUF=SX_RPC_CORE_MED_PRIO_BUF,

    SX_RPC_CORE_NUM_OF_PRIO_BUFS = (SX_RPC_CORE_MAX_PRIO_BUF+1),

    SX_RPC_CORE_NUM_OF_LOWER_PRIO_BUFS = SX_RPC_CORE_MAX_PRIO_BUF
}sx_rpc_core_prio_buffer_num_t;




typedef void (*sx_rpc_core_td_event_src_handler_t) (uint32_t idx);

typedef struct sx_rpc_core_td_event_src {
	const char *desc;
	sx_rpc_event_src_state_t state;
	uint8_t *buffer;
	uint32_t pos;
	cl_commchnl_t commchnl;
	sx_rpc_core_td_event_src_handler_t handler;
	pid_t sender_pid;
	/* uint8_t buf_type; */
} sx_rpc_core_td_event_src_t;

//typedef int (*api_command_fp_t)( rpc_core_td_event_src_t *,uint8_t *,uint32_t);

typedef struct sx_rpc_api_user_ctxt {
        cl_commchnl_t commchnl;
        cl_spinlock_t mutex;
        boolean_t valid;
} sx_rpc_api_user_ctxt_t;


typedef struct sx_rpc_mapped_api_command {
	cl_map_item_t 		map_item;
	sx_rpc_api_command_t 	cmd;
} sx_rpc_mapped_api_command_t;



typedef struct sx_rpc_core_worker_thread_prio_buf_context {
	cl_spinlock_t queue_lock_cons;
	cl_spinlock_t queue_lock_prod;
	sx_rpc_core_td_event_src_t **work_queue;
	uint32_t work_queue_cons;
	uint32_t work_queue_prod;
	uint32_t work_queue_size;
} sx_rpc_core_worker_thread_prio_buf_context_t;


typedef struct sx_rpc_core_td_worker_data {
	boolean_t active;
	cl_thread_t thread;
	//cl_event_t emad_event;
	cl_event_t work_queue_event;
	//uint32_t event_cnt;
	sx_rpc_core_worker_thread_prio_buf_context_t prio_buf_context[SX_RPC_CORE_NUM_OF_PRIO_BUFS];
	boolean_t worker_exit_signal_issued;
} sx_rpc_core_td_worker_data_t;

typedef enum {

	SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE=20,
	SX_RPC_CORE_MED_PRIO_BUFFER_SIZE=50,
	SX_RPC_CORE_LOW_PRIO_BUFFER_SIZE=20

}sx_rpc_event_buffer_sizes;

typedef struct sx_rpc_core_td_low_prio_event {
	sx_rpc_core_td_event_src_t levent;
	uint8_t buff[SX_RPC_API_LOW_PRIO_MESSAGE_SIZE_LIMIT+sizeof(sx_rpc_api_command_head_t)];
} sx_rpc_core_td_low_prio_event;

typedef struct sx_core_td_event_srcs_fd {
	fd_set fds_err;
	fd_set fds_in;
	uint32_t max_fd;
} sx_rpc_core_td_event_srcs_fd_t;


typedef int (*sx_rpc_core_worker_thread_arb_t)		(sx_rpc_core_td_worker_data_t **worker, uint8_t *cmd_body, uint32_t cmd_body_size, uint32_t opcode);


typedef struct sx_rpc_api_sx_sdk_versions {
	char sx_sdk[SX_RPC_API_VERSION_MAX_LEN]; /**< SDK version array*/
	char sx_api[SX_RPC_API_VERSION_MAX_LEN]; /**< API version array*/
	char sx_sxd[SX_RPC_API_VERSION_MAX_LEN]; /**< SXD version array*/
} sx_rpc_api_sx_sdk_versions_t;

typedef struct sx_rpc_server_database
{
	sx_rpc_core_td_event_src_t 				rpc_high_prio_event_srcs[SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE];
	sx_rpc_core_td_low_prio_event 			*rpc_med_low_prio_event_buffer[SX_RPC_CORE_NUM_OF_PRIO_BUFS];
	cl_spinlock_t 							rpc_event_src_state_lock;
	sx_rpc_core_td_event_srcs_fd_t 			rpc_event_srcs_fd;
	sx_rpc_core_td_worker_data_t 			rpc_td_worker[SX_RPC_CORE_TD_WORKER_NUM];
	sx_rpc_core_worker_thread_arb_t 		rpc_worker_arb_method;
	sx_rpc_api_sx_sdk_versions_t        	rpc_sx_core_versions;
	uint32_t								max_buffer_size;
}sx_rpc_server_database_t;


#endif //__SX_RPC_COMMON_DB_H__
