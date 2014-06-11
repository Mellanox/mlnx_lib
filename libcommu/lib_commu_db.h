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

#ifndef LIB_COMMU_DB_H_
#define LIB_COMMU_DB_H_

#include "lib_commu_log.h"
#include "lib_commu.h"
#include <netinet/in.h>

#ifdef LIB_COMMU_DB_C_

/************************************************
 *  Local Defines
 ***********************************************/

#define UDP_HANDLE_ARR_LENGTH       (16)
#define TCP_HANDLE_ARR_LENGTH       (16)
#define NUM_OF_LISTENER_THREADS     (16)
#define SERVER_STATUS_ARR_LENGTH    NUM_OF_LISTENER_THREADS
#define INVALID_MAGIC               UINT32_MAX
#define SOCKET_PATH_LEN             (50)
#define INVALID_SERVER_ID           SERVER_STATUS_ARR_LENGTH+1

/************************************************
 *  Local Macros
 ***********************************************/

/************************************************
 *  Local Type definitions
 ***********************************************/

enum db_type {
    UDP_HANDLE_DB = 1,      /**< indicating udp handles DB    */
    TCP_CLIENT_HANDLE_DB,   /**< indicating tcp handles DB    */
    TCP_SERVER_HANDLE_DB,   /**< indicating tcp opened by server handles DB    */
    TCP_SERVER_STATUS_HANDLE_DB,   /**< indicating tcp handles within server status DB */
    LISTENER_THREAD_DB,         /**< indicating open listener threads DB        */
    ANY_HANLDE_DB                         /**< any kind of DB - not specified*/
};

/**
 * handle_info structure is used to store
 * Information for each connection
 */
struct handle_info {
    handle_t handle;                            /**< the udp/tcp handle   */
    struct connection_info conn_info;           /**< message type to send   */
    struct socket_connection_info socekt_info;  /**< message type to send   */
};

struct listner_exit_event_info {
        int event_fd[NUM_OF_LISTENER_THREADS];
        char event_socket_path[NUM_OF_LISTENER_THREADS][SOCKET_PATH_LEN];
};

#endif

/************************************************
 *  Defines
 ***********************************************/

/************************************************
 *  Macros
 ***********************************************/

/**
 * A wrapper macro to 'free' which only calls 'free' if the variable
 * is not NULL and sets it to NULL after the call.
 */
#define safe_free(var)                                                       \
    do {                                                                     \
        if (var) {                                                           \
            free(var);                                                       \
            var = NULL;                                                      \
        }                                                                    \
    } while(0)

/************************************************
 *  Type definitions
 ***********************************************/

/************************************************
 *  Global variables
 ***********************************************/

extern pthread_t listener_threads[NUM_OF_LISTENER_THREADS];
extern struct listner_exit_event_info listner_exit_event_info_st;

/************************************************
 *  Function declarations
 ***********************************************/

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
comm_lib_db_verbosity_level_set(enum lib_commu_verbosity_level verbosity);

/**
 *  This function updates udp handle_info with new entries
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
                             uint32_t local_magic);

/**
 *  This function updates tcp handle_info with new entries
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
lib_commu_db_tcp_server_handle_info_set(uint16_t server_id, handle_t handle,
                                            uint8_t msg_type,
                                            struct addr_info local_info,
                                            struct addr_info peer_info,
                                            uint32_t local_magic);


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
                                            uint32_t local_magic);


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
                                        enum handle_op_status is_enabled);


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
                                      uint32_t s_ipv4_addr, uint16_t s_port,
                                      uint16_t server_idx);


/**
 *  This function fill up the server status struct
 *
 * @param[in,out] server_status - the server status
 *
 * @return 0 if operation completes successfully.
 * @return err != 0 if fails
 */
int
lib_commu_db_tcp_server_status_get(
        struct server_status const **server_status, uint16_t server_idx);


/**
 *  This function gets an unoccupied slot in listener thread array
 *
 * @param[in,out] idx - the unoccupied index in the array.
 *
 * @return 0 if operation completes successfully.
 * @return err != 0 if fails
 */
int
lib_commu_db_unoccupied_listener_thread_get(uint16_t *idx);


/**
 *  This function deletes handle_info by key handle
 *
 * @param[in] handle - socket handle
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
lib_commu_db_udp_handle_info_delete(handle_t handle);



/**
 *  This function deletes udp handle_info by key handle
 *
 * @param[in] handle - socket handle
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
lib_commu_db_tcp_handle_info_delete(handle_t handle);



/**
 *  This function gets handle_info by key handle
 *
 * @param[in] handle - socket handle
 * @param[in] handle_info - the information on the handle
 * @param[in,out] handle_db_type - if the handle_db_type is ANY then it will
 *                                 search in all handle DB and will return the type
 *                                 else - the specific DB
 * @param[in,out] server_id - the server id filled if the  handle_db_type == TCP_SERVER_HANDLE_DB
 *
 * @return 0 if operation completes successfully.
 * @return ENOKEY if didn't found handle in DB.
 */
int
lib_commu_db_hanlde_info_get(handle_t handle,
                             struct handle_info **handle_info_st,
                             enum db_type *handle_db_type,
                             uint16_t *server_id);

/**
 *  This function initialize the database
 *
 * @return 0 if operation completes successfully.
 * @return EPERM if Handle type doesn't exist.
 */
int
lib_commu_db_init(void);

/**
 *  This function deinitialize the database
 *
 * @return 0 if operation completes successfully.
 * @return EPERM if Handle type doesn't exist.
 */
int
lib_commu_db_deinit(void);

/**
 *  This function deinitialize the tcp database of specific server
 *
 * @return 0 if operation completes successfully.
 * @return err!=0 if operation completes unsuccessfully.
 */
int
lib_commu_db_tcp_session_db_deinit(uint16_t server_id);

/**
 *  This function update the total rx bytes sent on handle
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
                       unsigned long long rx_bytes);



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
                       unsigned long long tx_bytes);

#endif /* LIB_COMMU_DB_H_ */
