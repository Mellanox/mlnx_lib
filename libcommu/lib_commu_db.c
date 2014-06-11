/* Copyright (c) 2014  Mellanox Technologies, Ltd. All rights reserved.
 *
 * This software is available to you under BSD license below:
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
 */ 

#define LIB_COMMU_DB_C_

#include "lib_commu_db.h"
#include "lib_commu_log.h"
#include "lib_commu_bail.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>

#undef  __MODULE__
#define __MODULE__ LIB_COMMU_DB

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Local variables
 ***********************************************/

static struct handle_info udp_handle_info[UDP_HANDLE_ARR_LENGTH];
static struct handle_info server_tcp_handle_info[SERVER_STATUS_ARR_LENGTH][TCP_HANDLE_ARR_LENGTH];                                                                        /* opened by server side */
static struct handle_info client_tcp_handle_info[TCP_HANDLE_ARR_LENGTH]; /*open by connecting to another server*/
static struct server_status server_status_st[SERVER_STATUS_ARR_LENGTH];
static pthread_mutex_t lock_handles_db_access = PTHREAD_MUTEX_INITIALIZER;
static enum lib_commu_verbosity_level LOG_VAR_NAME(__MODULE__) =
        LCOMMU_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

/*
 *  This function initialize the udp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_udp_session_db_init(void);

/*
 *  This function deinitialize the udp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_udp_session_db_deinit(void);


/*
 *  This function initialize the tcp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_tcp_all_session_db_init(void);

/*
 *  This function deinitialize the tcp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_tcp_all_session_db_deinit(void);

/*
 *  This function initialize the mutex for the DB
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_mutex_init(void);

/*
 *  This function deinitialize the mutex for the DB
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_mutex_deinit(void);

static int
find_empty_slot(enum db_type type, uint16_t server_idx, uint16_t *idx);

static int
find_handle_idx(enum db_type handle_type, uint16_t server_id, handle_t handle,
                uint16_t *idx);

static int
reset_handle_db(enum db_type handle_type, uint16_t server_id);

static int
handle_info_set(enum db_type handle_type, uint16_t server_id, handle_t handle,
                uint8_t msg_type, struct addr_info local_info,
                struct addr_info peer_info, uint8_t is_single_peer,
                uint32_t local_magic);

static int
reset_all_tcp_servers_status_db(void);

static int
reset_server_status(uint16_t idx);

static int
init_listner_exit_event_info(void);


/************************************************
 *  Local function implementations
 ***********************************************/

static int
find_empty_slot(enum db_type type, uint16_t server_idx, uint16_t *idx)
{
    int err = 0;
    uint16_t i = 0;

    lib_commu_bail_null(idx);

    switch (type) {

        case UDP_HANDLE_DB:
            for (i = 0; i < UDP_HANDLE_ARR_LENGTH; i++) {
                if (udp_handle_info[i].handle == INVALID_HANDLE_ID) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        case TCP_CLIENT_HANDLE_DB:
            for (i = 0; i < TCP_HANDLE_ARR_LENGTH; i++) {
                if (client_tcp_handle_info[i].handle == INVALID_HANDLE_ID) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        case TCP_SERVER_HANDLE_DB:
            for (i = 0; i < TCP_HANDLE_ARR_LENGTH; i++) {
            if (server_tcp_handle_info[server_idx][i].handle ==
                INVALID_HANDLE_ID) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        case TCP_SERVER_STATUS_HANDLE_DB:
            for (i = 0; i < MAX_CONNECTION_NUM; i++) {
            if (server_status_st[server_idx].client_status[i].handle ==
                INVALID_HANDLE_ID) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        case LISTENER_THREAD_DB:
            for (i = 0; i < NUM_OF_LISTENER_THREADS; i++) {
                if (listener_threads[i] == 0) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        default:
            LCM_LOG(LCOMMU_LOG_ERROR, "Unknown DB type[%d]", type);
            lib_commu_bail_force(EINVAL);
            break;
    }

    LCM_LOG(LCOMMU_LOG_ERROR, "No empty slot in handle DB type[%d]", type);
    lib_commu_bail_force(ENOBUFS);


bail:
    return err;
}


static int
find_handle_idx(enum db_type handle_type, uint16_t server_id, handle_t handle,
                uint16_t *idx)
{
    int err = 0;
    uint16_t i = 0;

    lib_commu_bail_null(idx);

    switch (handle_type) {
        case UDP_HANDLE_DB:
            for (i = 0; i < UDP_HANDLE_ARR_LENGTH; i++) {
                if (udp_handle_info[i].handle == handle) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        case TCP_SERVER_STATUS_HANDLE_DB:
            for (i = 0; i < MAX_CONNECTION_NUM; i++) {
            if (server_status_st[server_id].client_status[i].handle ==
                handle) {
                    *idx = i;
                    goto bail;
                }
            }
            break;

        default:
            LCM_LOG(LCOMMU_LOG_ERROR, "handle type[%d] is unsupported",
                    handle_type);
            lib_commu_bail_error(EPERM);
            break;
    }


    LCM_LOG(LCOMMU_LOG_ERROR, "handle[%d] not found", handle);
    lib_commu_bail_error(ENOKEY);

bail:
    return err;
}

static int
reset_handle_db(enum db_type handle_type, uint16_t server_id)
{
    int err = 0;
    int i = 0;
    uint16_t idx = server_id;

    switch (handle_type) {

        case UDP_HANDLE_DB:

            memset(udp_handle_info, 0, sizeof(udp_handle_info));

            for (i = 0; i < UDP_HANDLE_ARR_LENGTH; i++) {
                udp_handle_info[i].handle = INVALID_HANDLE_ID;
                udp_handle_info[i].socekt_info.peer_magic = INVALID_MAGIC;
            }
            break;

        case TCP_CLIENT_HANDLE_DB:

        memset(client_tcp_handle_info, 0,
               sizeof(client_tcp_handle_info));

            for (i = 0; i < TCP_HANDLE_ARR_LENGTH; i++) {
                client_tcp_handle_info[i].handle = INVALID_HANDLE_ID;
                client_tcp_handle_info[i].socekt_info.peer_magic = INVALID_MAGIC;
            }
            break;

        case TCP_SERVER_HANDLE_DB:

        memset(server_tcp_handle_info[idx], 0,
               sizeof(server_tcp_handle_info[idx]));

            for (i = 0; i < TCP_HANDLE_ARR_LENGTH; i++) {
                server_tcp_handle_info[idx][i].handle = INVALID_HANDLE_ID;
            server_tcp_handle_info[idx][i].socekt_info.peer_magic =
                INVALID_MAGIC;
            }
            break;

        default:
            LCM_LOG(LCOMMU_LOG_ERROR, "Unknown handle type[%d]", handle_type);
            lib_commu_bail_force(EPERM);
            break;
    }


bail:
    return err;
}

static int
handle_info_set(enum db_type handle_type, uint16_t server_id,
                           handle_t handle, uint8_t msg_type,
                           struct addr_info local_info,
                           struct addr_info peer_info, uint8_t is_single_peer,
                           uint32_t local_magic)
{

    int err = 0;
    int is_db_locked = 0;
    uint16_t idx = 0;
    struct handle_info *handle_info = NULL;

    err = pthread_mutex_lock(&lock_handles_db_access);
    lib_commu_bail_error(err);
    is_db_locked = 1;

    err = find_empty_slot(handle_type, server_id, &idx);
    lib_commu_bail_error(err);

    switch (handle_type) {

        case UDP_HANDLE_DB:
            handle_info = &udp_handle_info[idx];
            break;

        case TCP_CLIENT_HANDLE_DB:
            handle_info = &client_tcp_handle_info[idx];
            break;

        case TCP_SERVER_HANDLE_DB:
            /* handle initiated by server (listener thread) */
            handle_info = &server_tcp_handle_info[server_id][idx];
            break;

        default:
            LCM_LOG(LCOMMU_LOG_ERROR, "Unknown handle type[%d]", handle_type);
            lib_commu_bail_force(EPERM);
            break;
    }

    /* set the handle with data */
    lib_commu_bail_null(handle_info);
    handle_info->handle = handle;
    handle_info->conn_info.msg_type = msg_type;
    handle_info->conn_info.s_ipv4_addr = local_info.ipv4_addr;
    handle_info->conn_info.s_port = local_info.port;
    handle_info->conn_info.d_ipv4_addr = peer_info.ipv4_addr;
    handle_info->conn_info.d_port = peer_info.port;
    handle_info->socekt_info.socket = handle;
    handle_info->socekt_info.local_magic = local_magic;
    handle_info->socekt_info.peer_magic = INVALID_MAGIC;
    handle_info->socekt_info.is_single_peer = is_single_peer;
    handle_info->socekt_info.total_sum_bytes_rx = 0;
    handle_info->socekt_info.total_sum_bytes_tx = 0;

    err = pthread_mutex_unlock(&lock_handles_db_access);
    lib_commu_bail_error(err);
    is_db_locked = 0;

bail:
    if (is_db_locked) {
        err = pthread_mutex_unlock(&lock_handles_db_access);
        lib_commu_return_from_bail(err);
    }
    return err;
}


static int
reset_all_tcp_servers_status_db(void)
{
    int err = 0;
    uint16_t i = 0;

    for (i = 0; i < SERVER_STATUS_ARR_LENGTH; i++) {
        err = reset_server_status(i);
        lib_commu_bail_error(err);

        err = reset_handle_db(TCP_SERVER_HANDLE_DB, i);
        lib_commu_bail_error(err);
    }

bail:
    return err;
}

static int
reset_server_status(uint16_t idx)
{
    int err = 0;
    int i = 0;

    memset(&(server_status_st[idx]), 0, sizeof(server_status_st[idx]));

    server_status_st[idx].listener_handle = INVALID_HANDLE_ID;
    server_status_st[idx].clients_num = 0;

    for (i = 0; i < MAX_CONNECTION_NUM; i++ ) {
        server_status_st[idx].client_status[i].handle = INVALID_HANDLE_ID;
    }

    goto bail; /*for compilation propose*/

bail:
    return err;
}


static int
init_listner_exit_event_info(void)
{
    int err = 0;
    uint16_t i = 0;

    memset(listner_exit_event_info_st.event_fd, INVALID_HANDLE_ID,
           sizeof(listner_exit_event_info_st.event_fd));

    char *exit_listener_thread_socket_path[] = {
                                          "/tmp/lib_commu_sock_path_0",
                                          "/tmp/lib_commu_sock_path_1",
                                          "/tmp/lib_commu_sock_path_2",
                                          "/tmp/lib_commu_sock_path_3",
                                          "/tmp/lib_commu_sock_path_4",
                                          "/tmp/lib_commu_sock_path_5",
                                          "/tmp/lib_commu_sock_path_6",
                                          "/tmp/lib_commu_sock_path_7",
                                          "/tmp/lib_commu_sock_path_8",
                                          "/tmp/lib_commu_sock_path_9",
                                          "/tmp/lib_commu_sock_path_10",
                                          "/tmp/lib_commu_sock_path_11",
                                          "/tmp/lib_commu_sock_path_12",
                                          "/tmp/lib_commu_sock_path_13",
                                          "/tmp/lib_commu_sock_path_14",
                                          "/tmp/lib_commu_sock_path_15",
    };

    for (i = 0; i < NUM_OF_LISTENER_THREADS; i++) {
        strcpy(listner_exit_event_info_st.event_socket_path[i],
               exit_listener_thread_socket_path[i]);
    }

    goto bail; /*for compilation propose*/

bail:
    return err;
}

/************************************************
 *  Function implementations
 ***********************************************/

/**
 *  This function initialize the database
 *
 * @return 0 if operation completes successfully.
 * @return EPERM if Handle type doesn't exist.
 */
int
lib_commu_db_init(void)
{
    int err = 0;

    err = lib_commu_db_udp_session_db_init();
    lib_commu_bail_error(err);

    err = lib_commu_db_tcp_all_session_db_init();
    lib_commu_bail_error(err);

    err = lib_commu_db_mutex_init();
    lib_commu_bail_error(err);

bail:
    return err;
}

/**
 *  This function deinitialize the database
 *
 * @return 0 if operation completes successfully.
 * @return EPERM if Handle type doesn't exist.
 */
int
lib_commu_db_deinit(void)
{
    int err = 0;

    err = lib_commu_db_udp_session_db_deinit();
    lib_commu_bail_error(err);

    err = lib_commu_db_tcp_all_session_db_deinit();
    lib_commu_bail_error(err);

    err = lib_commu_db_mutex_deinit();
    lib_commu_bail_error(err);

bail:
    return err;
}

/**
 *  This function initialize the udp database
 *
 * @return 0 if operation completes successfully.
 * @return EPERM if Handle type doesn't exist.
 */
static int
lib_commu_db_udp_session_db_init(void)
{
    int err = 0;

    err = reset_handle_db(UDP_HANDLE_DB, INVALID_SERVER_ID);
    lib_commu_bail_error(err);

bail:
    return err;
}

/**
 *  This function deinitialize the udp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_udp_session_db_deinit(void)
{
    int err = 0;

    err = reset_handle_db(UDP_HANDLE_DB, INVALID_SERVER_ID);
    lib_commu_bail_error(err);

bail:
    return err;
}


/**
 *  This function initialize the tcp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_tcp_all_session_db_init(void)
{
    int err = 0;

    err = reset_all_tcp_servers_status_db();
    lib_commu_bail_error(err);

    err = reset_handle_db(TCP_CLIENT_HANDLE_DB, INVALID_SERVER_ID);
    lib_commu_bail_error(err);

    err = init_listner_exit_event_info();
    lib_commu_bail_error(err);


bail:
    return err;
}

/**
 *  This function deinitialize the all tcp database
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
static int
lib_commu_db_tcp_all_session_db_deinit(void)
{
    int err = 0;

    err = reset_all_tcp_servers_status_db();
    lib_commu_bail_error(err);

    err = reset_handle_db(TCP_CLIENT_HANDLE_DB, INVALID_SERVER_ID);
    lib_commu_bail_error(err);

bail:
    return err;
}

int
lib_commu_db_tcp_session_db_deinit(uint16_t server_id)
{
    int err = 0;

    err = reset_server_status(server_id);
    lib_commu_bail_error(err);

    err = reset_handle_db(TCP_SERVER_HANDLE_DB, server_id);
    lib_commu_bail_error(err);

bail:
    return err;
}

static int
lib_commu_db_mutex_init(void)
{
    int err = 0;

    if (pthread_mutex_init(&lock_handles_db_access, NULL) < 0) {
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

static int
lib_commu_db_mutex_deinit(void)
{
    int err = 0;
    if (pthread_mutex_destroy(&lock_handles_db_access) < 0) {
        /*printf("pthread_mutex_destroy failed on lock_handles_db_access err(%u):", err);*/
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

/**
 * Sets verbosity level of communication library DB module
 *
 * @param[in] - verbosity - verbosity level
 * @param[in, out] - None
 * @param[out] - None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - Invalid argument - param out of range, if operation completes unsuccessfully
  */
int
comm_lib_db_verbosity_level_set(enum lib_commu_verbosity_level verbosity)
{
    int err = 0;

    if ((verbosity > LCOMMU_VERBOSITY_LEVEL_MIN) &&
        (verbosity <= LCOMMU_VERBOSITY_LEVEL_MAX)) {
        LOG_VAR_NAME(__MODULE__) = verbosity;
    }
    else {
        LCM_LOG(LCOMMU_LOG_ERROR, "verbosity[%d] is out of range <%d-%d>\n",
                verbosity, LCOMMU_VERBOSITY_LEVEL_MIN,
                LCOMMU_VERBOSITY_LEVEL_MAX);
        lib_commu_bail_force(EINVAL);
    }

bail:
    return err;
}

/**
 *  This function updates handle_info with new entries
 *
 * @param[in] handle - socket handle
 * @param[in] params - udp parameters needed initialize the session
 * @param[in] local_magic - the magic identifier of the socket
 *
 * @return 0 if operation completes successfully.
 * @return ENOBUFS if no space left in udp_handle_info DB.
 * @return EPERM if operation failed.
 */
int
lib_commu_db_udp_handle_info_set(handle_t handle,
                                 const struct ip_udp_params const *params,
                                 uint32_t local_magic)
{
    int err = 0;

    struct addr_info local_info;
    struct addr_info peer_info;

    memset(&local_info, 0, sizeof(local_info));
    memset(&peer_info, 0, sizeof(peer_info));

    local_info.ipv4_addr = params->session_params.port;
    local_info.port = params->session_params.port;

    err = handle_info_set(UDP_HANDLE_DB, INVALID_SERVER_ID,handle,
                          params->session_params.msg_type,
                          local_info, peer_info, params->is_single_peer,
                          local_magic);
    lib_commu_bail_error(err);

bail:
    return err;
}


/**
 *  This function updates server tcp handle_info with new entries created by
 *  listener socket
 *
 * @param[in] server_id - the server id
 * @param[in] handle - socket handle
 * @param[in] msg_type - type of message to send/receive on the socket
 * @param[in] port - port number bounded to the socket
 * @param[in] local_magic - the magic identifier of the socket
 *
 * @return 0 if operation completes successfully.
 * @return ENOBUFS if no space left in udp_handle_info DB.
 * @return EPERM if operation failed.
 */
int
lib_commu_db_tcp_server_handle_info_set(uint16_t server_id, handle_t handle,
                                            uint8_t msg_type,
                                            struct addr_info local_info,
                                            struct addr_info peer_info,
                                            uint32_t local_magic)
{
    int err = 0;

    err = handle_info_set(TCP_SERVER_HANDLE_DB, server_id, handle, msg_type,
                          local_info, peer_info, 1, local_magic);
    lib_commu_bail_error(err);

bail:
    return err;
}


/**
 *  This function updates server tcp handle_info with new entries
 *  initiated by client
 *
 * @param[in] handle - socket handle
 * @param[in] msg_type - type of message to send/receive on the socket
 * @param[in] port - port number bounded to the socket
 * @param[in] local_magic - the magic identifier of the socket
 *
 * @return 0 if operation completes successfully.
 * @return ENOBUFS if no space left in udp_handle_info DB.
 * @return EPERM if operation failed.
 */
int
lib_commu_db_tcp_client_handle_info_set(handle_t handle, uint8_t msg_type,
                                            struct addr_info local_info,
                                            struct addr_info peer_info,
                                            uint32_t local_magic)
{
    int err = 0;

    err = handle_info_set(TCP_CLIENT_HANDLE_DB, INVALID_SERVER_ID, handle,
                          msg_type, local_info, peer_info, 1, local_magic);
    lib_commu_bail_error(err);

bail:
    return err;
}

/**
 *  This function set the listener handle
 *
 * @param[in] listener_handle - listener socket handle.
 *
 * @return 0 if operation completes successfully.
 */
int
lib_commu_db_server_status_listener_set(uint16_t server_id,
                                            handle_t listener_handle,
                                            enum handle_op_status is_enabled)
{
    int err = 0;

    server_status_st[server_id].listener_handle = listener_handle;
    server_status_st[server_id].listener_status = is_enabled;

    goto bail; /*for compilation propose*/

bail:
    return err;
}


/**
 *  This function set new client into server DB
 *
 * @param[in] handle - client socket handle, which connected to server.
 * @param[in] clientaddr - client information
 * @param[in] s_ipv4_addr - source ipv4 (local)
 * @param[in] s_port - source port
 * @param[in] server_idx - server id
 * @return 0 if operation completes successfully.
 */
int
lib_cummo_db_server_status_client_set(handle_t handle,
                                          struct sockaddr_in *clientaddr,
                                          uint32_t s_ipv4_addr,
                                          uint16_t s_port,
                                          uint16_t server_idx)
{
    int err = 0;
    uint16_t handle_idx = 0;
    err =
        find_empty_slot(TCP_SERVER_STATUS_HANDLE_DB, server_idx, &handle_idx);
    lib_commu_bail_error(err);

    server_status_st[server_idx].clients_num += 1;
    server_status_st[server_idx].client_status[handle_idx].handle = handle;
    server_status_st[server_idx].client_status[handle_idx].handle_status =
            HANDLE_STATUS_UP;
    server_status_st[server_idx].client_status[handle_idx].local_ipv4_addr =
        s_ipv4_addr;
    server_status_st[server_idx].client_status[handle_idx].local_port = s_port;
    server_status_st[server_idx].client_status[handle_idx].peer_ipv4_addr =
            clientaddr->sin_addr.s_addr;
    server_status_st[server_idx].client_status[handle_idx].peer_port =
            clientaddr->sin_port;


bail:
    return err;
}

/**
 *  This function fill up the server status struct
 *
 * @param[in,out] server_status - the server status
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if server does not exist - id out of bound
 */
int
lib_commu_db_tcp_server_status_get(
        struct server_status const **server_status, uint16_t server_idx)
{
    int err = 0;
    int peer_err = 0;
    int i = 0;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    lib_commu_bail_null(server_status);

    if(server_idx >= SERVER_STATUS_ARR_LENGTH) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid parameter [server_idx]\n");
        lib_commu_bail_force(ENOKEY);
    }


    for (i = 0; i < MAX_CONNECTION_NUM; i++) {
        if (server_status_st[server_idx].client_status[i].handle !=
            INVALID_HANDLE_ID) {
            peer_err = getpeername(
                    server_status_st[server_idx].client_status[i].handle,
                    (struct sockaddr *) &client_addr, &addr_len);
            if (peer_err != 0) {
                if (errno == ENOTCONN) {
                    server_status_st[server_idx].client_status[i].handle_status
                        = HANDLE_STATUS_DOWN;
                    continue;
                }
                LCM_LOG(LCOMMU_LOG_ERROR, "getpeername() failed with err(%d)\n", peer_err);
                lib_commu_bail_force(peer_err);
            }
            server_status_st[server_idx].client_status[i].handle_status =
                    HANDLE_STATUS_UP;
        }
    }


    *server_status = &(server_status_st[server_idx]);

bail:
    return err;
}

/**
 *  This function gets an unoccupied slot in listener thread array
 *
 * @param[in,out] idx - the unoccupied index in the array.
 *
 * @return 0 if operation completes successfully.
 * @return ENOBUFS - if can't find unoccupied listener in DB
 */
int
lib_commu_db_unoccupied_listener_thread_get(uint16_t *idx)
{
    int err = 0;

    lib_commu_bail_null(idx);

    err = find_empty_slot(LISTENER_THREAD_DB, INVALID_SERVER_ID ,idx);
    lib_commu_bail_error(err);

bail:
    return err;
}

/**
 *  This function deletes udp handle_info by key handle
 *
 * @param[in] handle - socket handle
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
lib_commu_db_udp_handle_info_delete(handle_t handle)
{
    int err = 0;
    uint16_t idx = 0;

    err = find_handle_idx(UDP_HANDLE_DB, INVALID_SERVER_ID, handle, &idx);
    lib_commu_bail_error(err);

    memset(&udp_handle_info[idx], 0, sizeof(udp_handle_info[idx]));
    udp_handle_info[idx].handle = INVALID_HANDLE_ID;
    udp_handle_info[idx].socekt_info.peer_magic = INVALID_MAGIC;

bail:
    return err;
}


/**
 *  This function deletes tcp handle_info by key handle
 *  The supported DB types: TCP_CLIENT_HANDLE_DB && TCP_SERVER_HANDLE_DB
 *
 * @param[in] handle - socket handle
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 * @return EINVAL if can't find this handle
 * @return EPERM unsupported DB type
 */
int
lib_commu_db_tcp_handle_info_delete(handle_t handle)
{
    int err = 0;
    uint16_t server_id = 0;
    uint16_t idx = 0;
    struct handle_info *handle_info_st = NULL;
    enum db_type handle_db_type = ANY_HANLDE_DB;

    /* 1. get handle info */
    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st,
                                       &handle_db_type,
                                       &server_id);
    lib_commu_bail_error(err);
    lib_commu_bail_null(handle_info_st);

    if ((handle_db_type == TCP_CLIENT_HANDLE_DB) ||
            (handle_db_type == TCP_SERVER_HANDLE_DB)) {
        /* reset handle DB */
        memset((char*) handle_info_st, 0, sizeof(*handle_info_st));
        handle_info_st->handle = INVALID_HANDLE_ID;
        handle_info_st->socekt_info.peer_magic = INVALID_MAGIC;

        /* if handle initiated by server --> update it's DB */
        if (handle_db_type == TCP_SERVER_HANDLE_DB) {
            if (server_status_st[server_id].clients_num > 0) {
                server_status_st[server_id].clients_num--;
                err = find_handle_idx(TCP_SERVER_STATUS_HANDLE_DB, server_id,
                                      handle, &idx);
                lib_commu_bail_error(err);
                memset(&server_status_st[server_id].client_status[idx], 0,
                       sizeof(server_status_st[server_id].client_status[idx]));
                server_status_st[server_id].client_status[idx].handle =
                        INVALID_HANDLE_ID;
            }
            else { /* programming error: number of clients must be > 0 else no handle can be delete */
                LCM_LOG(LCOMMU_LOG_ERROR, "#clients must be > 0\n");
                lib_commu_bail_error(EINVAL);
            }
        }

        /* finish reset all DB --> goto bail */
        goto bail;
    }

    /* unsupported DB type */
    LCM_LOG(LCOMMU_LOG_ERROR, "Invalid handle type[%d]", handle_db_type);
    lib_commu_bail_force(EPERM);


bail:
    return err;
}


/**
 *  This function gets handle_info by key handle
 *
 * @param[in] handle - socket handle
 * @param[in,out] handle_info - the information on the handle
 * @param[in,out] handle_db_type - if the handle_db_type is ANY then it will
 *                                 search in all handle DB and will return the type
 *                                 else - the specific DB
 * @param[in,out] server_id - the server id filled if the  handle_db_type == TCP_SERVER_HANDLE_DB
 *                            may be NULL
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
lib_commu_db_hanlde_info_get(handle_t handle,
                                 struct handle_info **handle_info_st,
                                 enum db_type *handle_db_type,
                                 uint16_t *server_id)
{
    int err = 0;
    int i = 0, j = 0;

    lib_commu_bail_null(handle_info_st);
    lib_commu_bail_null(handle_db_type);

    switch(*handle_db_type) {

        case ANY_HANLDE_DB:

        case TCP_CLIENT_HANDLE_DB:
            for (i = 0; i < TCP_HANDLE_ARR_LENGTH; i++) {
                if (client_tcp_handle_info[i].handle == handle) {
                    *handle_info_st = &(client_tcp_handle_info[i]);
                    *handle_db_type = TCP_CLIENT_HANDLE_DB;
                    goto bail;
                }
            }
            if(*handle_db_type != ANY_HANLDE_DB) {
                break;
            }

        case TCP_SERVER_HANDLE_DB:
            for (j = 0; j < SERVER_STATUS_ARR_LENGTH; j++) {
                if (server_status_st[j].clients_num > 0) {
                    for (i = 0; i < TCP_HANDLE_ARR_LENGTH; i++) {
                        if (server_tcp_handle_info[j][i].handle == handle) {
                            *handle_info_st = &(server_tcp_handle_info[j][i]);
                            *handle_db_type = TCP_SERVER_HANDLE_DB;
                            if(server_id != NULL) { /*sometimes server_id is not needed */
                                *server_id = j;
                            }
                            goto bail;
                        }
                    }
                }
            }
            if (*handle_db_type != ANY_HANLDE_DB) {
                break;
            }

        case UDP_HANDLE_DB:
            for (i = 0; i < UDP_HANDLE_ARR_LENGTH; i++) {
                if (udp_handle_info[i].handle == handle) {
                    *handle_info_st = &(udp_handle_info[i]);
                    *handle_db_type = UDP_HANDLE_DB;
                    goto bail;
                }
            }
            if(*handle_db_type != ANY_HANLDE_DB) {
                break;
            }

        default:
            LCM_LOG(LCOMMU_LOG_ERROR, "handle[%d] not found", handle);
            lib_commu_bail_force(EINVAL);
            break;
    }

    LCM_LOG(LCOMMU_LOG_ERROR, "handle[%d] not found", handle);
    lib_commu_bail_error(ENOKEY);

bail:
    return err;
}


/**
 *  This function update the total rx bytes received on handle
 *
 * @param[in] handle - socket handle
 * @param[in] handle_db_type - the handle type {TCP(client/server), UDP}
 * @param[in] rx_bytes - number of bytes to add to total_rx
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
handle_total_rx_update(handle_t handle, enum db_type *handle_db_type,
                       unsigned long long rx_bytes)
{
    int err = 0;

    struct handle_info *handle_info_st = NULL;

    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st, handle_db_type,
                                       NULL);
    lib_commu_bail_error(err);

    handle_info_st->socekt_info.total_sum_bytes_rx += rx_bytes;

bail:
    return err;
}


/**
 *  This function update the total tx bytes sent on handle
 *
 * @param[in] handle - socket handle
 * @param[in] handle_db_type - the handle type {TCP(client/server), UDP}
 * @param[in] tx_bytes - number of bytes to add to total_tx
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
handle_total_tx_update(handle_t handle, enum db_type *handle_db_type,
                       unsigned long long tx_bytes)
{
    int err = 0;

    struct handle_info *handle_info_st = NULL;

    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st, handle_db_type,
                                       NULL);
    lib_commu_bail_error(err);

    handle_info_st->socekt_info.total_sum_bytes_tx += tx_bytes;

bail:
    return err;
}
