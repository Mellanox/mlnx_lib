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

#include <complib/cl_init.h>
#include <complib/cl_types.h>
#include <complib/cl_commchnl.h>
#include <complib/cl_spinlock.h>
#include <complib/cl_thread.h>
#include <complib/cl_mem.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <complib/sx_rpc.h>
#include "sx_rpc_common_db.h"


#undef  __MODULE__
#define __MODULE__ RPC

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_NOTICE;

/**
 * server data structure of
 */
static sx_rpc_server_database_t rpc_server_db;

static boolean_t rpc_log_initialized = FALSE;

#define SX_RPC_LOG(level, format, ...)				\
do								\
{								\
	if (rpc_log_initialized)				\
		SX_LOG(level,format, ## __VA_ARGS__);		\
} while(0)

static void sx_rpc_update_fd_sets(void);


static boolean_t main_exit_signal_issued=FALSE;


static sx_rpc_comlib_db_data sx_rpc_cmd_db_data;


void sx_rpc_register_log_function(sx_log_cb_t log_cb)
{
	sx_log_init(TRUE, NULL, log_cb);
	rpc_log_initialized = TRUE;
}

/*
 * release the mutex
 */
static void sx_rpc_release_cmd_table_access()
{
	cl_plock_release(&(sx_rpc_cmd_db_data.cmd_db_mutex));
};


void sx_rpc_change_main_exit_signal_state(boolean_t bool)
{
	main_exit_signal_issued = bool;
}

/*
 * This function loads the command type and the pointer function
 * every pointer function represent by command type
 */
static int sx_rpc_core_api_command_set(const sx_rpc_api_command_t *cmd_data)
{
	sx_rpc_mapped_api_command_t *entry = NULL;
	cl_map_item_t *map_item = NULL;
	const cl_map_item_t *map_end = NULL;
	int err = SX_RPC_STATUS_SUCCESS;

	cl_plock_excl_acquire(&(sx_rpc_cmd_db_data.cmd_db_mutex));

	/* get entry from tree according to command id */
	map_item = cl_qmap_get(&(sx_rpc_cmd_db_data.cmd_map), cmd_data->cmd_id);
	map_end = cl_qmap_end(&(sx_rpc_cmd_db_data.cmd_map));

	if (map_item == map_end) {
		entry = cl_malloc(sizeof(sx_rpc_mapped_api_command_t));
		if (entry == NULL) {
			SX_RPC_LOG(SX_LOG_ERROR,"Memory allocation failed.\n");
			goto out;
		}

		/* insert the entry to tree */
		memcpy(&(entry->cmd), cmd_data, sizeof(sx_rpc_api_command_t));
		cl_qmap_insert(&(sx_rpc_cmd_db_data.cmd_map), cmd_data->cmd_id,
		               &(entry->map_item));
	} else {
		entry = PARENT_STRUCT(map_item, sx_rpc_mapped_api_command_t, map_item);
		memcpy(&(entry->cmd), cmd_data, sizeof(sx_rpc_api_command_t));
	}

out:
	sx_rpc_release_cmd_table_access();
	return err;
}

/*
 * This function loop over all command type and call to
 * sx_rpc_core_api_command_set loading entries.
 */
static int
sx_rpc_cmd_table_init(sx_rpc_api_command_t *sx_core_api_cmd_table_p,
                      uint32_t num_of_item)
{
	uint32_t i = 0;
	int err = SX_RPC_STATUS_SUCCESS;

	for(i = 0; i < num_of_item; i++)
	{
		err = sx_rpc_core_api_command_set(&sx_core_api_cmd_table_p[i]);
		if (err != 0)
		{
			SX_RPC_LOG(SX_LOG_ERROR,"sx_rpc_core_api_command_set failed.\n");
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}
	}

out:
	return err;
}

static int
sx_rpc_cmd_table_deinit()
{
	sx_rpc_mapped_api_command_t *entry = NULL;
	cl_map_item_t *map_item = NULL;
	cl_map_item_t *next_map_item = NULL;
	const cl_map_item_t *map_end = NULL;
	int err = SX_RPC_STATUS_SUCCESS;

	cl_plock_excl_acquire(&(sx_rpc_cmd_db_data.cmd_db_mutex));

	map_item = cl_qmap_head(&sx_rpc_cmd_db_data.cmd_map);
	map_end = cl_qmap_end(&sx_rpc_cmd_db_data.cmd_map);

	while (map_item != map_end) {
		next_map_item = cl_qmap_next(map_item);
		cl_qmap_remove_item(&(sx_rpc_cmd_db_data.cmd_map), map_item);

		entry = PARENT_STRUCT(map_item, sx_rpc_mapped_api_command_t, map_item);
		cl_free(entry);

		map_item = next_map_item;
	}

	sx_rpc_release_cmd_table_access();
	return err;
}

sx_rpc_status_t
sx_rpc_api_rpc_command_db_init(sx_rpc_api_command_t *sx_core_api_cmd_table_p,
                               uint32_t num_of_item)
{
	cl_status_t cl_err=0;
	int err=SX_RPC_STATUS_SUCCESS;

	cl_qmap_init(&(sx_rpc_cmd_db_data.cmd_map));

	cl_err = cl_plock_init(&(sx_rpc_cmd_db_data.cmd_db_mutex));
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "cl_plock_init failed: %u.\n", cl_err);
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}

	/* loading the table of callback function to tree */
	err = sx_rpc_cmd_table_init(sx_core_api_cmd_table_p, num_of_item);
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "sx_rpc_cmd_table_init failed.\n");
		goto out;
	}
out:
	return err;
}

sx_rpc_status_t
sx_rpc_api_rpc_command_db_deinit()
{
	int err = SX_RPC_STATUS_SUCCESS;

	err = sx_rpc_cmd_table_deinit();
	if (err != SX_RPC_STATUS_SUCCESS) {
		SX_RPC_LOG(SX_LOG_ERROR, "sx_rpc_cmd_table_deinit failed.\n");
		goto out;
	}

out:
	return err;
}

/*
 * this function send message client/server
 */
static int rpc_send_message(cl_commchnl_t *commchnl, uint8_t *header, uint32_t header_size,uint8_t *body, uint32_t body_size)
{
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;


	/* send header message */
	cl_err = cl_commchnl_send(commchnl, header, &header_size);
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Failed header message send at communication channel: %s\n", strerror(errno));
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}

	SX_RPC_LOG(SX_LOG_INFO, "succeeded to header send client message\n");

	if (body != NULL)
	{
		SX_RPC_LOG(SX_LOG_INFO, "body message include %d len\n",body_size);
		/* send body message */
		cl_err = cl_commchnl_send(commchnl, body, &body_size);
		if (cl_err != CL_SUCCESS)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "Failed body message send at communication channel: %s\n", strerror(errno));
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}
		SX_RPC_LOG(SX_LOG_INFO, "succeeded to send body message\n");
	}

out:
	return err;
}

/*
 * this function send reply message to client
 */
static int sx_rpc_api_send_reply(cl_commchnl_t *commchnl, sx_rpc_api_reply_head_t *reply_head, uint8_t *reply_body)
{
	int err = SX_RPC_STATUS_SUCCESS;

	err = rpc_send_message(commchnl, (uint8_t *)reply_head, sizeof(sx_rpc_api_reply_head_t),
			     reply_body, reply_head->msg_size - sizeof(sx_rpc_api_reply_head_t));


	return err;
}
/*
 * this function add header to message and call to sx_api_send_reply sending message
 */
static int sx_rpc_api_send_reply_wrapper(cl_commchnl_t *commchnl, int retcode,	uint8_t *reply_body, uint32_t reply_body_size)
{
	sx_rpc_api_reply_head_t reply_head;
	int err = SX_RPC_STATUS_SUCCESS;


	memset(&reply_head,0,sizeof(sx_rpc_api_reply_head_t));
	reply_head.retcode = retcode;
	/* reply_head.version = SX_RPC_API_INT_VERSION; */
	reply_head.msg_size = sizeof(sx_rpc_api_reply_head_t) + reply_body_size;

	err = sx_rpc_api_send_reply(commchnl, &reply_head, reply_body);

	return err;
}
/*
 * this function search for data by command id key
 */
static int sx_rpc_core_api_command_get(int cmd_id,sx_rpc_api_command_t *cmd_data)
{
	int sx_status = SX_RPC_STATUS_SUCCESS;
	cl_map_item_t *map_item_p = NULL;
	sx_rpc_mapped_api_command_t *cmd;

	cl_plock_acquire(&(sx_rpc_cmd_db_data.cmd_db_mutex));

	map_item_p = cl_qmap_get(&(sx_rpc_cmd_db_data.cmd_map), (uint64_t)cmd_id);

	if (map_item_p == &sx_rpc_cmd_db_data.cmd_map.nil) {
		SX_RPC_LOG(SX_LOG_ERROR,"command %u not found in api_cmd_db\n", cmd_id);
		sx_status = SX_RPC_STATUS_ERROR;
		goto end;
	}

	cmd = (sx_rpc_mapped_api_command_t *)map_item_p;
	memcpy(cmd_data,&(cmd->cmd),sizeof(sx_rpc_api_command_t));

end:
	sx_rpc_release_cmd_table_access();
	return sx_status;
}
/*
 * this function handle the event received from RPC
 */
static int
sx_rpc_core_api_dispatch(sx_rpc_core_td_event_src_t *event, uint8_t buffer_type)
{
	sx_rpc_api_command_head_t *cmd_head = NULL;
	uint8_t *cmd_body = NULL;
	uint32_t cmd_body_size = 0;
	sx_rpc_api_command_t cmd_data;
	int err = SX_RPC_STATUS_SUCCESS;
	uint8_t *cmd_body_reply = NULL;
	uint32_t cmd_body_size_reply = 0;


	cmd_head = (sx_rpc_api_command_head_t *) (event->buffer);
	event->sender_pid = cmd_head->pid;

	if (buffer_type == SX_RPC_CORE_HIGH_PRIO_BUF)
	{
/* don't check version
		if (cmd_head->version != SX_RPC_API_INT_VERSION)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "SX-API client request with wrong API version. "
			       "Received [%u], Expected [%u].\n", cmd_head->version, SX_RPC_API_INT_VERSION);
			err = sx_rpc_api_send_reply_wrapper(&(event->commchnl), 1, NULL, 0);
			goto out;
		}
*/
		/* if request to close the session */
		if (cmd_head->opcode == SX_RPC_API_CMD_CLOSE_EVENT_SRC)
		{
			SX_RPC_LOG(SX_LOG_DEBUG,"client disconnection identified\n");
			err = SX_RPC_STATUS_INT_COMM_CLOSE;
			goto out;
		}

		SX_RPC_LOG(SX_LOG_INFO, "client request, opcode [%x], version [%u], "
		       "size [%u], list [%u].\n", cmd_head->opcode, cmd_head->version,
		       cmd_head->msg_size, cmd_head->list_size);
	}

	cmd_body = event->buffer + sizeof(sx_rpc_api_command_head_t);
	cmd_body_size = cmd_head->msg_size - sizeof(sx_rpc_api_command_head_t);

	/* get the command from tree */
	err = sx_rpc_core_api_command_get(cmd_head->opcode, &cmd_data);
	if (err != 0)
	{
		SX_RPC_LOG(SX_LOG_INFO, "client command %u is not supported\n ", cmd_head->opcode);
		err = sx_rpc_api_send_reply_wrapper(&(event->commchnl), err, NULL, 0);
		goto out;
	}

	/* call to callback function - it could be server fill some data and here we will call to to wrapper */
	err = cmd_data.func(cmd_body, cmd_body_size,&cmd_body_reply,&cmd_body_size_reply);
	if(err == SX_RPC_STATUS_SUCCESS)
	{
		/* return good reply */
		err = sx_rpc_api_send_reply_wrapper(&(event->commchnl), err, cmd_body_reply, cmd_body_size_reply);
		if(err != SX_RPC_STATUS_SUCCESS)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "failed to reply message %d\n", cmd_head->opcode);
		}
		else
		{
			SX_RPC_LOG(SX_LOG_INFO, "succeeded to reply message %d\n", cmd_head->opcode);
		}
	}
	else
	{
		err = sx_rpc_api_send_reply_wrapper(&(event->commchnl), err, NULL, 0);
	}

out:
	return err;
}

/*
 * close the session
 */
static void close_event_src(sx_rpc_core_td_event_src_t *event)
{

	cl_spinlock_acquire(&rpc_server_db.rpc_event_src_state_lock);

	if (event->state != SX_RPC_EVENT_SRC_STATE_NOT_ACTIVE)
	{
		cl_commchnl_destroy(&(event->commchnl));
		event->state = SX_RPC_EVENT_SRC_STATE_NOT_ACTIVE;
		sx_rpc_update_fd_sets();
	}
	cl_spinlock_release(&rpc_server_db.rpc_event_src_state_lock);

}

/*
 * this is the main function wait on event from client and respond
 */
static void sx_rpc_worker_thread(void *context)
{
	int32_t worker_index = (int32_t)(uintptr_t)context;
	sx_rpc_core_td_worker_data_t *data = &(rpc_server_db.rpc_td_worker[worker_index]);
	sx_rpc_core_td_event_src_t *event = NULL;
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;
	int buff_idx;
	boolean_t need_to_wait=TRUE;;
	sx_rpc_core_worker_thread_prio_buf_context_t *prio_buf;

	SX_RPC_LOG(SX_LOG_INFO, "worker thread started\n");

	/* wait on event from complib if the comm lib ready */
	cl_err = cl_event_wait_on(&(data->work_queue_event), EVENT_NO_TIMEOUT, TRUE);
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Error at work queue event wait\n");
		goto out;
	}

	while(TRUE)
	{

		/* check if time to die */
		/* if received signal and need to close the session */
		if (rpc_server_db.rpc_td_worker[worker_index].worker_exit_signal_issued == TRUE)
		{
			SX_RPC_LOG(SX_LOG_DEBUG, "Thread __worker_thread (index %d) is gracefully ending.\n", worker_index);
			return;
		}

		/* loop over all priority start from high priority */
		for(buff_idx = SX_RPC_CORE_HIGH_PRIO_BUF; buff_idx >= SX_RPC_CORE_LOW_PRIO_BUF; buff_idx--)
		{
			prio_buf = &(data->prio_buf_context[buff_idx]);

			need_to_wait = TRUE;

			cl_spinlock_acquire(&(prio_buf->queue_lock_prod));

			/* get the next wqe */
			if(prio_buf->work_queue_cons != prio_buf->work_queue_prod) {

				cl_spinlock_release(&(prio_buf->queue_lock_prod));
				SX_RPC_LOG(SX_LOG_INFO, "Event #%u buffer #%u taken by worker thread #%u\n",prio_buf->work_queue_cons, buff_idx, 0);
				event = prio_buf->work_queue[prio_buf->work_queue_cons];

				need_to_wait = FALSE;

				/* handle the message */
				err = sx_rpc_core_api_dispatch(event,(uint8_t)buff_idx);

				switch (err)
				{
				case SX_RPC_STATUS_SUCCESS :
					SX_RPC_LOG(SX_LOG_INFO, "request completed\n");
					break;
				case SX_RPC_STATUS_INT_COMM_CLOSE :
					SX_RPC_LOG(SX_LOG_INFO, "client closed , error : [%d]\n",err);
					close_event_src(event);
					break;
				default :
					SX_RPC_LOG(SX_LOG_ERROR, "client closed , error : [%d]\n",err);
					close_event_src(event);
					break;
				}

				cl_spinlock_acquire(&(prio_buf->queue_lock_cons));

				prio_buf->work_queue_cons = (prio_buf->work_queue_cons + 1) % prio_buf->work_queue_size;

				cl_spinlock_release(&(prio_buf->queue_lock_cons));

				SX_RPC_LOG(SX_LOG_INFO, "Work queue for buffer %u(cons, prod) - (%u, %u)\n",buff_idx, prio_buf->work_queue_cons, prio_buf->work_queue_prod);

				break;

			}
			else
			{
				cl_spinlock_release(&(prio_buf->queue_lock_prod));
			}

		}  /* for */

		if (need_to_wait)
		{
			SX_RPC_LOG(SX_LOG_INFO, "wait on event queue\n");
			/* Waiting point - wait on event queue */
			cl_err = cl_event_wait_on(&(data->work_queue_event), EVENT_NO_TIMEOUT, TRUE);
			if (cl_err != CL_SUCCESS)
			{
				SX_RPC_LOG(SX_LOG_ERROR, "Error at work queue event wait\n");
				goto out;
			}

			/* check if time to die (in case need to wait) */
			if (rpc_server_db.rpc_td_worker[worker_index].worker_exit_signal_issued == TRUE)
			{
				SX_RPC_LOG(SX_LOG_ERROR,"Thread __worker_thread (index %d) is gracefully ending.\n", worker_index);
				return;
			}

		} /* need to wait */

	} /* forever */

out:
	return;
}

static void sx_rpc_select_event()
{
	fd_set fds_in;
	fd_set fds_err;
	int i = 0, ret = SX_RPC_STATUS_SUCCESS;

	cl_spinlock_acquire(&rpc_server_db.rpc_event_src_state_lock);
	fds_in = rpc_server_db.rpc_event_srcs_fd.fds_in;
	fds_err = rpc_server_db.rpc_event_srcs_fd.fds_err;
	cl_spinlock_release(&rpc_server_db.rpc_event_src_state_lock);

	ret = select(rpc_server_db.rpc_event_srcs_fd.max_fd + 1, &fds_in, NULL, &fds_err, NULL);

	/* Check if it is time to die */
	if (main_exit_signal_issued == TRUE)
	{
		/* Stop inserting API requests to the system */
		while(1)
		{
			SX_RPC_LOG(SX_LOG_INFO, "Main loop moved to termination mode - waiting for Exit...\n");
			sleep(10);  /* sleep 10 second */
		}
	}

	if (ret > 0)
	{
		for (i = 0; i < SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE; ++i)
		{
			int commchnl_fd = 0;

			if (rpc_server_db.rpc_high_prio_event_srcs[i].state != SX_RPC_EVENT_SRC_STATE_ACTIVE)
			{
				continue;
			}

			commchnl_fd = cl_commchnl_get_fd(&(rpc_server_db.rpc_high_prio_event_srcs[i].commchnl));
			if (FD_ISSET(commchnl_fd, &fds_err))
			{
				SX_RPC_LOG(SX_LOG_ERROR, "Receive error on %s communication channel: %s\n",
					   rpc_server_db.rpc_high_prio_event_srcs[i].desc,
					   strerror(errno));
				/* TODO: Add error handle for the specific client */
			}
			else if (FD_ISSET(commchnl_fd, &fds_in))
			{
				SX_RPC_LOG(SX_LOG_INFO, "Receive event on %s communication channel\n",
					   rpc_server_db.rpc_high_prio_event_srcs[i].desc);

				/* TODO:
				 *	Right now we serve SX-API client requests serially.
				 *	We should see if we could handle them simultanouselly (add worker thread(s))
				 *	and add locks in case of mutual exlusive operations.
				 */
				rpc_server_db.rpc_high_prio_event_srcs[i].handler(i);
			}
		}
	}
	else if (ret == -1)
	{
        if (errno == EINTR)
        	SX_RPC_LOG(SX_LOG_DEBUG, "Select system call was interrupted\n");
        else
        	SX_RPC_LOG(SX_LOG_ERROR, "Select failed: %s\n", strerror(errno));
	}
}

void sx_rpc_core_td_start_loop(void)
{

	while(1) {
		sx_rpc_select_event();
	}
	return;
}

/*
 * this function init the server side and call to create thread
 */
static int sx_rpc_open_sx_worker_td(void)
{
	uint32_t i,j = 0;
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;
	sx_rpc_core_prio_buffer_num_t buff_idx;
	sx_rpc_core_worker_thread_prio_buf_context_t *prio_buf;


	/* loop over priority buffer and allocate memory */
	for(i=0; i < SX_RPC_CORE_TD_WORKER_NUM; ++i)
	{

		rpc_server_db.rpc_td_worker[i].worker_exit_signal_issued = FALSE;
		rpc_server_db.rpc_td_worker[i].thread.osd.id = (pthread_t)NULL;

		cl_err = cl_event_init(&(rpc_server_db.rpc_td_worker[i].work_queue_event), FALSE);
		if (cl_err != CL_SUCCESS)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "Could not initialize work queue event\n");
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}

		rpc_server_db.rpc_td_worker[i].prio_buf_context[SX_RPC_CORE_HIGH_PRIO_BUF].work_queue_size 	= SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE;
		rpc_server_db.rpc_td_worker[i].prio_buf_context[SX_RPC_CORE_MED_PRIO_BUF].work_queue_size 	= SX_RPC_CORE_MED_PRIO_BUFFER_SIZE;
		rpc_server_db.rpc_td_worker[i].prio_buf_context[SX_RPC_CORE_LOW_PRIO_BUF].work_queue_size 	= SX_RPC_CORE_LOW_PRIO_BUFFER_SIZE;

		for(buff_idx=SX_RPC_CORE_MIN_PRIO_BUF; buff_idx <= SX_RPC_CORE_MAX_PRIO_BUF; buff_idx++)
		{
			prio_buf = &(rpc_server_db.rpc_td_worker[i].prio_buf_context[buff_idx]);

			prio_buf->work_queue = cl_malloc(sizeof(sx_rpc_core_td_event_src_t *)*prio_buf->work_queue_size);
			if (!(prio_buf->work_queue) )
			{
				err = SX_RPC_STATUS_NO_RESOURCES;
				SX_RPC_LOG(SX_LOG_ERROR,"%d buffer pointer memory alloc fail\n",buff_idx);
				goto out;
			}
			memset((prio_buf->work_queue),0,(prio_buf->work_queue_size)*sizeof(sx_rpc_core_td_event_src_t *));

			prio_buf->work_queue_cons = 0;
			prio_buf->work_queue_prod = 0;

			cl_spinlock_init(&(prio_buf->queue_lock_cons));
			cl_spinlock_init(&(prio_buf->queue_lock_prod));


		}

		cl_err = cl_thread_init(&(rpc_server_db.rpc_td_worker[i].thread),sx_rpc_worker_thread, (void *)(uintptr_t)i, NULL);
		if (cl_err != CL_SUCCESS)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "Could not create thread\n");
			rpc_server_db.rpc_td_worker[i].thread.osd.id = (pthread_t)NULL;
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}

		rpc_server_db.rpc_td_worker[i].active = TRUE;

		SX_RPC_LOG(SX_LOG_INFO, "RPC worker thread #%u opened\n", i);

	}

   	for(buff_idx=SX_RPC_CORE_MIN_PRIO_BUF; buff_idx <= SX_RPC_CORE_MAX_NON_HIGH_PRIO_BUF; buff_idx++)
   	{
		rpc_server_db.rpc_med_low_prio_event_buffer[buff_idx] = cl_malloc(sizeof(sx_rpc_core_td_low_prio_event)*(rpc_server_db.rpc_td_worker[0].prio_buf_context[buff_idx].work_queue_size));
		if (!(rpc_server_db.rpc_med_low_prio_event_buffer[buff_idx]) )
		{
				err = SX_RPC_STATUS_ERROR;
				SX_RPC_LOG(SX_LOG_ERROR,"%d buffer memory alloc fail\n",buff_idx);
				goto out;
			}

		for(j=0; j < rpc_server_db.rpc_td_worker[0].prio_buf_context[buff_idx].work_queue_size; j++)
		{
			rpc_server_db.rpc_med_low_prio_event_buffer[buff_idx][j].levent.buffer = rpc_server_db.rpc_med_low_prio_event_buffer[buff_idx][j].buff;
			rpc_server_db.rpc_td_worker[0].prio_buf_context[buff_idx].work_queue[j] = &(rpc_server_db.rpc_med_low_prio_event_buffer[buff_idx][j].levent);
		}
   	}

out:
	return err;
}

static int sx_rpc_static_allocate_event_src(uint32_t max_message_size)
{
	uint32_t i = 0;
	int err = SX_RPC_STATUS_SUCCESS;


	cl_spinlock_init(&rpc_server_db.rpc_event_src_state_lock);

	for (;i < SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE;++i)
	{
		rpc_server_db.rpc_high_prio_event_srcs[i].buffer = cl_malloc(max_message_size * sizeof(uint8_t));

		if (rpc_server_db.rpc_high_prio_event_srcs[i].buffer == NULL)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "Failed to malloc event src buffer for client %u\n",i);
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}
	}

out:
	return err;
}

static int	sx_rpc_core_worker_thread_fixed_arb(sx_rpc_core_td_worker_data_t** worker,
											uint8_t *cmd_body,
											uint32_t cmd_body_size,
											uint32_t opcode)
{

	int err = SX_RPC_STATUS_SUCCESS;

	UNUSED_PARAM(cmd_body);
	UNUSED_PARAM(cmd_body_size);
	UNUSED_PARAM(opcode);

	*worker = &rpc_server_db.rpc_td_worker[0];

	return err;
}

/*
 * init the event procedure
 */
static void sx_rpc_init_event_src(sx_rpc_core_td_event_src_t *event, cl_commchnl_t *commchnl,
		 const char *desc, sx_rpc_core_td_event_src_handler_t handler)
{

	event->pos = 0;
	event->desc = desc;
	event->commchnl = *commchnl;
	event->handler = handler;
	event->state = SX_RPC_EVENT_SRC_STATE_ACTIVE;

}

/*
 * create file descriptor for every high priority
 */
static void sx_rpc_update_fd_sets(void)
{
	int i = 0;


	FD_ZERO(&(rpc_server_db.rpc_event_srcs_fd.fds_in));
	FD_ZERO(&(rpc_server_db.rpc_event_srcs_fd.fds_err));
	rpc_server_db.rpc_event_srcs_fd.max_fd = 0;

	for (i = 0; i < SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE; ++i) {
		if (rpc_server_db.rpc_high_prio_event_srcs[i].state == SX_RPC_EVENT_SRC_STATE_ACTIVE)
		{
			int commchnl_fd = cl_commchnl_get_fd(&(rpc_server_db.rpc_high_prio_event_srcs[i].commchnl));
			FD_SET((unsigned int)commchnl_fd, &(rpc_server_db.rpc_event_srcs_fd.fds_in));
			FD_SET((unsigned int)commchnl_fd, &(rpc_server_db.rpc_event_srcs_fd.fds_err));
			rpc_server_db.rpc_event_srcs_fd.max_fd = MAX((int)rpc_server_db.rpc_event_srcs_fd.max_fd, commchnl_fd);
		}
	}

}

/*
 * init the events
 */
static int sx_rpc_add_event_src(cl_commchnl_t *commchnl, const char *desc, sx_rpc_core_td_event_src_handler_t handler)
{
	int i;
	int err = SX_RPC_STATUS_SUCCESS;


	cl_spinlock_acquire(&rpc_server_db.rpc_event_src_state_lock);

	for (i = 0; i < SX_RPC_CORE_HIGH_PRIO_BUFFER_SIZE; ++i)
	{
		if (rpc_server_db.rpc_high_prio_event_srcs[i].state == SX_RPC_EVENT_SRC_STATE_NOT_ACTIVE)
		{
			sx_rpc_init_event_src(rpc_server_db.rpc_high_prio_event_srcs + i, commchnl, desc, handler);
			sx_rpc_update_fd_sets();
			goto out;
		}
	}

	err = 1;

out:
	cl_spinlock_release(&rpc_server_db.rpc_event_src_state_lock);
	return err;
}

static int sx_rpc_api_receive_command(cl_commchnl_t *commchnl, uint8_t *buffer, uint32_t *buffer_position)
{
	uint32_t i = 0, msg_size = 0, buffer_size = 0;
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;


	for (i = 0; i < 2; ++i)
	{
		if (i == 0)
		{
			msg_size = sizeof(sx_rpc_api_command_head_t);
		}
		else
		{
			msg_size = ((sx_rpc_api_command_head_t *)(buffer))->msg_size;
			CL_ASSERT(msg_size <= rpc_server_db.max_buffer_size);
		}

		if (*buffer_position < msg_size)
		{
			buffer_size = msg_size - *buffer_position;
			cl_err = cl_commchnl_recv(commchnl, FALSE, buffer + *buffer_position, &buffer_size);
			if (cl_err == CL_DISCONNECT)
			{
				SX_RPC_LOG(SX_LOG_DEBUG, "Connection closed\n");
				err = SX_RPC_STATUS_ERROR;
				goto out;
			}
			else if (cl_err != CL_SUCCESS)
			{
				SX_RPC_LOG(SX_LOG_ERROR, "Failed command read at communication channel: %s\n",
				       strerror(errno));
				err = SX_RPC_STATUS_ERROR;
				goto out;
			}

			*buffer_position += buffer_size;
			if (*buffer_position < msg_size)
			{
				err = SX_RPC_STATUS_CMD_INCOMPLETE;
				goto out;
			}
		}
	}

	*buffer_position = 0;
out:
	return err;
}

static void sx_rpc_client_handler(uint32_t idx)
{
	sx_rpc_core_td_event_src_t *event = &rpc_server_db.rpc_high_prio_event_srcs[idx];
	sx_rpc_core_td_worker_data_t *worker_td = NULL;
	int err = SX_RPC_STATUS_SUCCESS;
	sx_rpc_api_command_head_t *close_cmd = NULL;
	sx_rpc_api_command_head_t *cmd_head = NULL;
	sx_rpc_api_command_t cmd_data;
	sx_rpc_core_worker_thread_prio_buf_context_t *prio_buf;


	err = sx_rpc_api_receive_command(&(event->commchnl), event->buffer, &(event->pos));
	if (err == SX_RPC_STATUS_CMD_INCOMPLETE)
	{
		goto out;
	}
	else if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_DEBUG, "Failed to receive from client, Disconnecting...\n");
		/* Insert msg to close event src. Will be done by Worker thread.*/
		cl_spinlock_acquire(&rpc_server_db.rpc_event_src_state_lock);
		event->state = SX_RPC_EVENT_SRC_STATE_TERMINATING;
		sx_rpc_update_fd_sets();
		cl_spinlock_release(&rpc_server_db.rpc_event_src_state_lock);
		close_cmd = (sx_rpc_api_command_head_t *)event->buffer;
		close_cmd->opcode = SX_RPC_API_CMD_CLOSE_EVENT_SRC;
		/* close_cmd->version = SX_RPC_API_INT_VERSION; */
		close_cmd->msg_size = sizeof(sx_rpc_api_command_head_t);
		close_cmd->list_size = 0;
	}

	cmd_head = (sx_rpc_api_command_head_t *)event->buffer;

	if (cmd_head->opcode != SX_RPC_API_CMD_CLOSE_EVENT_SRC)
	{
		err = sx_rpc_core_api_command_get(cmd_head->opcode, &cmd_data);
		if (err != SX_RPC_STATUS_SUCCESS)
		{
			SX_RPC_LOG(SX_LOG_INFO, "client command %u is not supported\n ", cmd_head->opcode);
			err = sx_rpc_api_send_reply_wrapper(&(event->commchnl), 1, NULL, 0);
			goto out;
		}
	}
	else
	{
		cmd_data.priority = SX_RPC_API_CMD_PRIO_HIGH;
	}

	err = rpc_server_db.rpc_worker_arb_method(&worker_td, event->buffer + sizeof(sx_rpc_api_command_head_t),
				cmd_head->msg_size - sizeof(sx_rpc_api_command_head_t), cmd_head->opcode);
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR,"worker arbitration failed");
		goto out;
	}

	prio_buf = &(worker_td->prio_buf_context[cmd_data.priority]);

	cl_spinlock_acquire(&(prio_buf->queue_lock_prod));

	prio_buf->work_queue[prio_buf->work_queue_prod] = &(rpc_server_db.rpc_high_prio_event_srcs[idx]);
	SX_RPC_LOG(SX_LOG_INFO, "Event #%u passed into worker thread #%u\n", idx, 0);

	prio_buf->work_queue_prod = (prio_buf->work_queue_prod + 1) % prio_buf->work_queue_size;
	SX_RPC_LOG(SX_LOG_INFO, "Work queue (cons, prod) - (%u, %u)\n", prio_buf->work_queue_cons, prio_buf->work_queue_prod);

	cl_spinlock_release(&(prio_buf->queue_lock_prod));
	cl_event_signal(&(worker_td->work_queue_event));

out:
	return;
}

static void sx_rpc_accept_connection(uint32_t idx)
{
	sx_rpc_core_td_event_src_t *event = &rpc_server_db.rpc_high_prio_event_srcs[idx];
	cl_commchnl_t commchnl_client;
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;
	uint32_t len;

	cl_err = cl_commchnl_accept(&(event->commchnl), &commchnl_client);

	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_WARNING, "Failed accepting new connection: %s\n", strerror(errno));
		goto out;
	}


	err = sx_rpc_add_event_src(&commchnl_client, "RPC client", sx_rpc_client_handler);
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_WARNING, "Could not add client communication channel into event array\n");
		cl_commchnl_destroy(&commchnl_client);
		goto out;
	}

	/* two-way handshake */
	len = sizeof(rpc_server_db.rpc_sx_core_versions);
	cl_err = cl_commchnl_send(&commchnl_client,(uint8_t *)&rpc_server_db.rpc_sx_core_versions,&len);
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_WARNING, "Failed sending to new connection: %s\n", strerror(errno));
		cl_commchnl_destroy(&commchnl_client);
		goto out;
	}

	SX_RPC_LOG(SX_LOG_INFO, "New connection accepted\n");

out:
	return;
}

static int sx_rpc_open_sx_api_commchnl(char *commmchannel_name)
{
	cl_commchnl_t commchnl;
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;


	/* init the communication channel server side */
	cl_err = cl_commchnl_init(&commchnl, commmchannel_name, CL_COMMCHNL_SIDE_SERVER);
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Could not open server communication channel\n");
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}

	err = sx_rpc_add_event_src(&commchnl, "RPC server",sx_rpc_accept_connection);
	if (err != 0)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Could not add RPC server communication channel into event array\n");
		cl_commchnl_destroy(&commchnl);
		goto out;
	}

	SX_RPC_LOG(SX_LOG_INFO, "RPC server is ready for connections\n");

out:
	return err;
}

sx_rpc_status_t
sx_rpc_core_td_init(char *commmchannel_name,uint32_t max_message_size)
{
	int err = SX_RPC_STATUS_SUCCESS;

	memset(&rpc_server_db.rpc_td_worker[0],0,sizeof(rpc_server_db.rpc_td_worker));
	memset(&rpc_server_db.rpc_high_prio_event_srcs[0],0,sizeof(rpc_server_db.rpc_high_prio_event_srcs));

	err = sx_rpc_static_allocate_event_src(max_message_size);
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Failed Init SX Event Source\n");
		goto out;
	}

	rpc_server_db.max_buffer_size = max_message_size;

	/* worker thread arbitration method */
	rpc_server_db.rpc_worker_arb_method = sx_rpc_core_worker_thread_fixed_arb;

	/* init the worker database */
	err = sx_rpc_open_sx_worker_td();
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Failed opening SX worker threads\n");
		goto out;
	}

	/* open communication channel */
	err = sx_rpc_open_sx_api_commchnl(commmchannel_name);
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Failed opening SX API communication cannel\n");
		goto out;
	}

out:
	return err;
}

/*************************************************************************************
 * 			client code
 *************************************************************************************/

static int sx_rpc_receive_message(cl_commchnl_t *commchnl, sx_rpc_api_reply_head_t *reply_head,
		  uint8_t *reply_body, uint32_t reply_body_size)
{
	uint32_t buffer_size = 0;
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;


	buffer_size = sizeof(sx_rpc_api_reply_head_t);
	cl_err = cl_commchnl_recv(commchnl, TRUE, (uint8_t *)reply_head, &buffer_size);
	if (cl_err == CL_DISCONNECT)
	{
		SX_RPC_LOG(SX_LOG_DEBUG, "Connection closed\n");
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}
	else if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Failed message receive at communication channel: %s\n",
		       strerror(errno));
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}

	SX_RPC_LOG(SX_LOG_INFO, "succeeded to received header message from server\n");


	buffer_size = reply_head->msg_size - sizeof(sx_rpc_api_reply_head_t);
	if (buffer_size)
	{
		/* SX SDK server may choose to reply shorter message */
		if (buffer_size > reply_body_size)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "Message size overflow.\n");
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}

		cl_err = cl_commchnl_recv(commchnl, TRUE, reply_body, &buffer_size);
		if (cl_err == CL_DISCONNECT)
		{
			SX_RPC_LOG(SX_LOG_DEBUG, "Connection closed\n");
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}
		else if (cl_err != CL_SUCCESS)
		{
			SX_RPC_LOG(SX_LOG_ERROR, "Failed message receive at communication channel: %s\n",
			       strerror(errno));
			err = SX_RPC_STATUS_ERROR;
			goto out;
		}
		SX_RPC_LOG(SX_LOG_INFO, "succeeded to received body message from server\n");
	}

out:
	return err;
}

sx_rpc_status_t
sx_rpc_send_command_decoupled(sx_rpc_handle_t handle,
			      sx_rpc_api_command_head_t *cmd_head,
			      uint8_t *cmd_body,
			      sx_rpc_api_reply_head_t *reply_head,
			      uint8_t *reply_body,
			      uint32_t reply_body_size)
{
	int err = SX_RPC_STATUS_SUCCESS;
	sx_rpc_api_user_ctxt_t *ctxt = (sx_rpc_api_user_ctxt_t *)(uintptr_t)handle;
	if (!ctxt) {
		SX_RPC_LOG(SX_LOG_ERROR,"Invalid handle: handle is NULL.\n");
		return SX_RPC_STATUS_INVALID_HANDLE;
	}

	if (ctxt->valid == FALSE)
	{
		SX_RPC_LOG(SX_LOG_ERROR,"Invalid handle: handle is not valid.\n");
		return SX_RPC_STATUS_INVALID_HANDLE;
	}

	cl_spinlock_acquire(&(ctxt->mutex));
	cmd_head->pid = ctxt->commchnl.pid;


	/* Send command */
	err = rpc_send_message(&(ctxt->commchnl), (uint8_t *)cmd_head, sizeof(sx_rpc_api_command_head_t),
			     cmd_body, cmd_head->msg_size - sizeof(sx_rpc_api_command_head_t));
	if (err != SX_RPC_STATUS_SUCCESS)
	{
		ctxt->valid = FALSE;
		goto out;
	}

	/* Receive reply */
	err = sx_rpc_receive_message(&(ctxt->commchnl), reply_head, reply_body, reply_body_size);
	if (err != 0)
	{
		ctxt->valid = FALSE;
		goto out;
	}

	err = (int)(reply_head->retcode);

out:
	cl_spinlock_release(&(ctxt->mutex));
	return err;
}

sx_rpc_status_t
sx_rpc_open(sx_rpc_handle_t *handle,
	    const char *commchannel_name)
{
	sx_rpc_api_sx_sdk_versions_t versions;
	uint32_t len = sizeof(versions);
	cl_status_t cl_err = CL_SUCCESS;
	int err = SX_RPC_STATUS_SUCCESS;
	sx_rpc_api_user_ctxt_t *ctxt=NULL;

	if (handle == NULL) {
		SX_RPC_LOG(SX_LOG_ERROR, "NULL handle\n");
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}
	*handle = SX_RPC_INVALID_HANDLE;

	ctxt = cl_malloc(sizeof(sx_rpc_api_user_ctxt_t));
	if (ctxt == NULL)
	{
		SX_RPC_LOG(SX_LOG_ERROR,"failed to allocate memory\n");
		err = SX_RPC_STATUS_ERROR;
		goto out;
	}

	/* Complib init is not needed as long as atomic operation and timers are not used */
	cl_err = cl_commchnl_init(&(ctxt->commchnl), commchannel_name,CL_COMMCHNL_SIDE_CLIENT);
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Could not open client channel communication channel.\n");
		err = SX_RPC_STATUS_ERROR;
		goto out_err;
	}

	cl_err = cl_spinlock_init(&(ctxt->mutex));
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Could not open client mutex\n");
		err = SX_RPC_STATUS_ERROR;
		goto out_commchnl;
	}

	cl_err = cl_commchnl_recv(&(ctxt->commchnl),TRUE,(uint8_t *)&versions,&len);
	if (cl_err != CL_SUCCESS)
	{
		SX_RPC_LOG(SX_LOG_ERROR, "Failed opening new connection: Out Of Resources.\n");
		err = SX_RPC_STATUS_ERROR;
		goto out_spinlock;
	}

	ctxt->valid = TRUE;
	*handle = (uint64_t)(uintptr_t)ctxt;

	SX_RPC_LOG(SX_LOG_INFO, "sx_rpc_open init succeeded pid %d socket %d\n",ctxt->commchnl.pid,ctxt->commchnl.socket);
	goto out;

out_spinlock:
	cl_spinlock_destroy(&(ctxt->mutex));

out_commchnl:
	cl_commchnl_destroy(&(ctxt->commchnl));
out_err:
    cl_free(ctxt);
out:
	return err;
}

sx_rpc_status_t
sx_rpc_close(sx_rpc_handle_t *handle)
{
	sx_rpc_api_user_ctxt_t *ctxt = NULL;

	if (handle == NULL) {
		SX_RPC_LOG(SX_LOG_ERROR, "NULL handle\n");
		return SX_RPC_STATUS_PARAM_NULL;
	}

	if (*handle == SX_RPC_INVALID_HANDLE) {
		SX_RPC_LOG(SX_LOG_ERROR, "Invalid handle\n");
		return SX_RPC_STATUS_INVALID_HANDLE;
	}

	ctxt = (sx_rpc_api_user_ctxt_t *)(uintptr_t)*handle;
	cl_spinlock_destroy(&(ctxt->mutex));
	cl_commchnl_destroy(&(ctxt->commchnl));
	    cl_free(ctxt);

	*handle = SX_RPC_INVALID_HANDLE;

	return SX_RPC_STATUS_SUCCESS;
}

