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

#ifndef LIB_COMMU_H_
#define LIB_COMMU_H_

#include <stdint.h>
#include <sys/un.h>
#include <sys/time.h>
#include "lib_commu_log.h"

struct addr_info {
    uint16_t port;        /**< the destination port   */
    uint32_t ipv4_addr;   /**< destination IPv4       */
};

typedef int handle_t;

typedef int (*handle_notification)(handle_t new_handle,
                                   struct addr_info peer_addr_st, void *data,
                                   int rc);

struct register_to_new_handle {
    handle_notification clbk_notify_func; /**< callback function to be called */
    void *data;                           /**< magic number for the callback - user defined input */
};

/**
 * session_params structure is used to set
 * new connection with IP address and port to bind
 */
struct session_params {
    uint16_t port;        /**< the source port - to be bound */
    uint32_t s_ipv4_addr; /**< source IPv4 - local address */
    uint16_t msg_type;    /**< the message type send on this connection   */
};

/**
 * connection_info_t structure is used to store
 * for each connection
 */
struct connection_info {
    uint32_t s_ipv4_addr;   /**< source IPv4            */
    uint16_t s_port;        /**< the source port        */
    uint32_t d_ipv4_addr; /**< destination IPv4         */
    uint16_t d_port;        /**< the destination port   */
    uint16_t msg_type;    /**< the message type send on this connection   */
};

#ifdef LIB_COMMU_C_

/************************************************
 *  Local Type definitions
 ***********************************************/

/**
 * connection_info_t structure is used to store
 * for each connection
 */
#pragma pack(push, 1)
struct msg_metadata {
    uint32_t payload_size; /**< payload size to send   */
    uint32_t trailer; /**< magic number to be used once connection is established */
    uint8_t version;      /**< version number   */
    uint8_t msg_type;     /**< message type to send   */
};
#pragma pack(pop)


struct listener_thread_args {
        int event_fd;
        int server_id;
        int thread_idx;
        int listener_fd;
        struct register_to_new_handle clbk_st;
        struct session_params params;
};

struct connect_thread_args {
        int client_fd;
        struct connection_info conn_info;
        struct register_to_new_handle clbk_st;
        struct timeval tval;
        int use_tval;
};


/************************************************
 *  Local Defines
 ***********************************************/

#define MSG_VERSION                 (1)
#define MAX_MTU                     (1500)
#define DEFAULT_UDP_SERVER_PORT     3100
#define DEFAULT_UDP_CLIENT_PORT     3200
#define MAX_IPV4_HEADER_SIZE        (60)
#define UDP_HEADER_SIZE             (8)
#define MAX_UDP_MSG_SIZE            (MAX_MTU - MAX_IPV4_HEADER_SIZE - \
                                     UDP_HEADER_SIZE)                                  /* not including sizeof(msg_metadata) */
#define PENDING_CONNECTIOS_SIZE     (16)
#define SEND_REPEAT_NUM             (500)
#define RECV_REPEAT_NUM             (500)
#define CLIENT_CONNECT_THREAD_PATH  "/tmp/lib_commu_client_connect"

/************************************************
 *  Local Macros
 ***********************************************/

#endif

/************************************************
 *  Defines
 ***********************************************/

#define MAX_MSGS            (20) /* TODO: define the proper max */
#define MAX_UDP_PAYLOAD     (1422) /* if metadata size change need to change it also */
#define MAX_TCP_PAYLOAD     (4094)
#define MAX_JUMBO_TCP_PAYLOAD     (1024*1600) /* ~1.63 MB */
#define MAX_CONNECTION_NUM  (16)
#define GENERAL_MSG_TYPE    (65535)
#define INVALID_HANDLE_ID   (-1)

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/

enum handle_op_status {
    HANDLE_STATUS_DOWN = 0,    /**< indicating the handle status as DOWN*/
    HANDLE_STATUS_UP = 1      /**< indicating the handle status as UP*/
};

/**
 * recv_payload_data structure is used to store
 * data received from handle
 */
struct recv_payload_data {
    uint32_t msg_num_recv; /**< #messages actual received */
    uint8_t msg_type[MAX_MSGS]; /**< the message type received */
    uint8_t payload[MAX_MSGS][MAX_TCP_PAYLOAD]; /**< the data received */
    uint32_t payload_len[MAX_MSGS]; /**< bytes actual received */
    uint8_t jumbo_payload[MAX_JUMBO_TCP_PAYLOAD]; /**< the (jumbo) data received */
    uint32_t jumbo_payload_len; /**< bytes actual received on jumbo payload */
    uint8_t jumbo_msg_type; /**< the jumbo message type received */
};

/**
 * socket_connection_info structure is used to store
 * server connection
 */
struct socket_connection_info {
    handle_t socket; /**< socket id */
    uint32_t state; /**< the state of the socket (UP/DOWN) */
    uint8_t is_single_peer; /**< magic number initialized when new connection is established. */
    uint32_t local_magic; /**< magic number initialized when new connection is established. */
    uint32_t peer_magic; /**< magic number initialized when first message comes from peer. */
    /** byte_stream_t rx_stream;     TBD */
    /** uint32 bytes_to_receive;     TBD */
    /** uint32 bytes_received;       TBD */
    /** byte_stream_t tx_stream;     TBD */
    /** uint32 bytes_to_send;        TBD */
    /** uint32 bytes_sent;           TBD */
    unsigned long long total_sum_bytes_rx; /**< total number of bytes received on this socket - statistics */
    unsigned long long total_sum_bytes_tx; /**< total number of bytes sent on this socket - statistics     */
};

/**
 * connection_status_t structure is used to store
 * the status of the connection.
 */
struct connection_status {
    handle_t handle; /**< the connection handle */
    enum handle_op_status handle_status; /**< the connection operational status */
    uint32_t local_ipv4_addr;   /**< the IP address of my side */
    uint16_t local_port;        /**< the port of my side */
    uint32_t peer_ipv4_addr;    /**< the IP address of my peer */
    uint16_t peer_port;         /**< the port of the peer */
};

/**
 * server_status_t structure is used to store
 * the status of the tcp server.
 */
struct server_status {
    uint32_t clients_num; /**< number of clients connected to server  */
    struct connection_status client_status[MAX_CONNECTION_NUM]; /**< client status */
    handle_t listener_handle; /**< the listener handle */
    uint8_t listener_status; /**< listener status enable==1/disable==0 */
};


/**
 * udp_role enum is used to determine if udp connection
 * acts as server or client
 */
enum udp_role {
    CONN_SERVER, /**< connection acts as server - binds the socket         */
    CONN_CLIENT, /**< connection acts as clinet - doesn't bind the socket, assumes client first use sendto  */
};


/**
 * ip_udp_params structure is used to start new udp connection
 */
struct ip_udp_params {
    struct session_params session_params;   /**<the params to start session */
    enum udp_role connection_role;           /**<the ip udp connection role (bind / don't bind socekt) */
    uint16_t is_single_peer;                /**<if 1 ==> enforce receive data from single peer */
};

/**
 * udp_fields union is used to determine which kind of udp connection
 * to start
 */
union udp_fields
{
    struct ip_udp_params ip_udp_params; /**<ip udp parameters      */
    /* mc_udp_params_t  mc_udp_params; multi-cast udp parameters   */
    /* raw_uc_prarms_t     raw_prarms; raw udp parameters          */
};

/**
 * udp_type enum is used to determine which kind of udp connection
 * to start
 */
enum udp_type {
    CONN_TYPE_UDP_IP_UC, /**< Unreliable unicast connection */
    /* CON_TYPE_IP_MC,       multicast connection */
    /* CON_TYPE_RAW_UC,      Build Layer 2 unicast connection */
};


/**
 * udp_params enum is used to determine which kind of udp connection
 * to start
 */
struct udp_params {
    enum udp_type type;       /**< the connection type        */
    union udp_fields fields;    /**< the connection parameters  */
};

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

/**
 * This function is used to open communication library and init it's data
 *
 * @param[in] logging_cb - the logging callback
 * @param[in,out] None
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EFAULT or EINVAL or EPERM if pseusdo random init failed
 * @return EPERM if DB operation failed
 * @return errno codes of native pthread_mutex_init function
 */
int
comm_lib_init(lib_commu_log_cb_t logging_cb);

/**
 * This function is used to close communication library and deinit it's data
 *
 * @param[in] None
 * @param[in,out] None
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EPERM if library didn't finish init
 * @return errno codes of native pthread_mutex_destroy function
 */
int
comm_lib_deinit();


/**
 * Sets verbosity level of communication library
 *
 * @param[in] verbosity - verbosity level
 * @param[in,out] None
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - Invalid argument, if operation completes unsuccessfully
 * @return EPERM if library didn't finish init
 */
int
comm_lib_verbosity_level_set(enum lib_commu_verbosity_level verbosity);


/**
 * Gets the verbosity level of communication library
 *
 * @param[in] verbosity - verbosity level
 * @param[in,out] None
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL Invalid argument - param null, if operation completes unsuccessfully
 * @return EPERM if library didn't finish init
 */
int
comm_lib_verbosity_level_get(enum lib_commu_verbosity_level *verbosity);


/**
 * start a TCP connection from the server side.
 * opens a thread which "listens" to new connections.
 * Once connection is established a callback is being made with
 * a new handle toward the client.
 *
 * @param[in] params - server address, port, etc (network order)
 * @param[in] clbk_st - function callback
 * @param[in,out] server_id - the id of the server to open
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL if clbk_st == NULL
 * @return EACCES if can't create listener socket
 * @return ENOBUFS or ENOMEM The socket cannot be created until sufficient resources are freed
 * @return EAGAIN if Insufficient resources to create listener thread
 * @return EPERM if library didn't finish init
 * @return ENOMEM if can't create new session due to DB limit
 */
int
comm_lib_tcp_server_session_start(struct session_params params,
                                  struct register_to_new_handle *clbk_st,
                                  uint16_t *server_id);


/**
 * stop a TCP connection from the server side.
 * close a thread which "listens" to new connections. It does close
 * all the open sockets within this thread.
 *
 * @param[in] server_id - the id of the server to close
 * @param[in] handle_array_len - size of handle_array
 * @param[in,out] uint32 *handle_array - the handles which closed.
 * @param[out] - None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - if handle_array == NULL or handle_array_len == 0
 * @return EBADF  or ENOBUFS if if operation completes unsuccessfully
 * @return EINTR or EIO - if failed to close socket.
 * @return ENOKEY if server_id is out of bound.
 * @return EPERM if library didn't finish init
 */
int
comm_lib_tcp_server_session_stop(uint16_t server_id, handle_t *handle_array,
                                 uint32_t handle_array_len);


/**
 * start a TCP connection from the client side. (Blocking)
 * opens a socket toward specific server IP and port from user context
 * Once connection is established the function returns and update new handle
 * IP and port must be valid
 *
 * @param[in] conn_info - server address, port, etc...
 * @param[in,out] client_handle
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - if client_handle or conn_info are NULL
 * @return EPERM if library didn't finish init
 * @return errno codes of native socket function
 * @return errno codes of native connect function
 */
int
comm_lib_tcp_client_blocking_start(
    const struct connection_info const *conn_info, handle_t *client_handle);


/**
 * start a TCP connection from the client side. (NON-Blocking)
 * opens a thread which opens a socket toward specific server IP and port
 * Once connection is established, a callback is being made with a new handle
 * toward the client.
 * IP and port must be valid
 *
 * @param[in] conn_info - server address, port, etc...
 * @param[in] clbk_st - function callback
 * @param[in] tval - Client can set timeout for the connection.
 *                   if tval == NULL, connection will wait till connection is established or error
 *                   else connection will wait till time stated will pass.
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - if client_handle or conn_info are NULL
 * @return EPERM if library didn't finish init
 * @return ETIMEDOUT if connection haven't been established till timeout.
 * @return errno codes of native socket function
 * @return errno codes of native connect function
 */
int
comm_lib_tcp_client_non_blocking_start(
        const struct connection_info const *conn_info,
        struct register_to_new_handle *clbk_st, struct timeval *tval);

/**
 * close a TCP connection between two peers.
 * Can be used from server/client side
 *
 * @param[in] handle
 * @param[in,out] None
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EPERM - handle haven't create by library TCP API[s]
 * @return EPERM if library didn't finish init
 */
int
comm_lib_tcp_peer_stop(handle_t handle);


/**
 * send the payload as stream of bytes over TCP connection
 * Can be used from server/clients side. blocking until all payload is sent.
 *
 * @param[in] payload - the data to pass.
 * @param[in,out] payload_len - size of payload
 * @param[out] None
 *
 * @return 0 if operation completes successfully
 * @return EOVERFLOW - if payload_len exceeds MAX TCP SIZE message
 * @return EINVAL or ENOKEY- if handle doesn't exist in library DB
 * @return EIO - if could not sent all buffer (after 3 retries)
 * @return EPERM if library didn't finish init
 * @return errno codes of native send function
 */
int
comm_lib_tcp_send_blocking(handle_t handle, uint8_t *payload,
                           uint32_t *payload_len);


/**
 * Receive the messages as stream of bytes over TCP connection
 * Can be used from server/clients side. blocking until at least one message is received.
 * payload_data->msg_num_recv > 0 when got message
 * if payload_data->jumbo_payload_len > 0 --> the message received on payload_data->jumbo_payload
 *
 * @param[in] max_msgs_to_recv - the maximum messages to receive (1-MAX_MSGS)
 * @param[in,out] addresser_st - server address, port.
 * @param[in,out] recv_payload_data_t - the data structure which contains parsed messages payloads must be initalize.
 * @param[out] - None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - if conn_info == NULL or payload_data == NULL or max_msgs_to_recv == 0 or handle type != TCP type
 * @return ECONNRESET or ENOTCONN - if connection on handle have been reset by peer i.e socket not active
 * @return EBADE - if msg type received on socket is invalid
 * @return EOVERFLOW - if message size > MAX_TCP_PAYLOAD or meesage size = 0
 * @return EPERM - if max_msgs_to_recv > 1, currently the API supports one message each time
 */
int
comm_lib_tcp_recv_blocking(handle_t handle, struct addr_info *addresser_st,
                           struct recv_payload_data *payload_data,
                           uint32_t max_msgs_to_recv);


/**
 * Get the server status.
 *
 * @param[in] server_id - the id of the server.
 * @param[in,out] server_status.
 * @param[out] None.
 *
 *
 * @return 0 if operation completes successfully
 * @return EINVAL if server_status == NULL
 * @return ENOKEY if server_id is out of bound.
 */
int
comm_lib_tcp_server_status_get(
    const struct server_status const **server_status, uint16_t server_id);


/**
 * Get the connection status by the handle given
 *
 * @param[in] None.
 * @param[in,out] connection_status
 * @param[out] None.
 *
 * Global Variables Referred & Modified:
 * client_handle_array - Referred
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - if connection_status == NULL or invalid handle given
 * @return EPERM if library didn't finish init
 * @return errno codes of native getpeername function
 * @return errno codes of native getsockname function
 */
int
comm_lib_tcp_handle_status_get(struct connection_status *connection_status);


/**
 * Open a UDP socket connection from specific types
 *
 * @param[in] params - parameters in order to open the socket.
 * @param[in,out] handle - the handle open by the connection
 * @param[out] - None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL Invalid argument - handle==null.
 * @return EKEYREJECTED Key was rejected by service - param.type is unsupported.
 * @return ENOBUFS or ENOMEM or EACCES is creation of socket fails. SOCKET(2) in Linux Programmer's Manual
 * @return EACCES or EADDRINUSE or EBADF or EINVAL and more  if binding the socket fails.
 * @return ENOBUFS if can't update session db with the new handle.
 * @return EPERM if library didn't finish init
 */
int
comm_lib_udp_session_start(handle_t *handle,
                           const struct udp_params const *params);


/**
 * Close a socket connection
 *
 * @param[in] handle - the handle to close connection.
 * @param[in,out] None.
 * @param[out] None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return EBADF if handle isn't a valid open file descriptor.
 * @return EINTR if The close() call was interrupted by a signal
 * @return EIO if An I/O error occurred.
 * @return EPERM if library didn't finish init
 */
int
comm_lib_udp_session_stop(handle_t handle);


/**
 * Send payload over a UDP connection
 *
 * @param[in] handle - the handle to send data.
 * @param[in] payload - data to send.
 * @param[in] recipient_st - the information of the recipient (address and port)
 * @param[in,out] payload_len - sizeof the payload, filled with actual #bytes sent.
 * @param[out] - None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return 0 if operation completes successfully
 * @return EOVERFLOW if payload_len is insufficient.
 * @return EIO if failed to receive any part of the message
 * @return EBADE if got data from unhallowed peer or corrupted data
 * @return EINVAL if payloand_len == 0 || payload == NULL
 * @return EPERM if library didn't finish init
 */
int
comm_lib_udp_send(handle_t handle, struct addr_info recipient_st,
                  uint8_t *payload, uint32_t *payload_len);


/**
 * Receive payload over a UDP connection
 *
 * @param[in] handle - the handle to receive the data
 * @param[in] payload - data received.
 * @param[in,out] payload_len - sizeof the payload, filled with actual #bytes received.
 * @param[in,out] addresser_st - the information of the addresser (address and port)
 * @param[out] None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return 0 if operation completes successfully
 * @return EOVERFLOW if payload_len is insufficient.
 * @return EIO if failed to receive any part of the message
 * @return EBADE if got data from unhallowed peer or corrupted data
 * @return EINVAL if payloand_len == 0 || payload
 * @return EPERM if library didn't finish init
 */
int
comm_lib_udp_recv(handle_t handle, struct addr_info *addresser_st,
                  uint8_t *payload, uint32_t *payload_len);

#endif /* LIB_COMMU_H_ */
