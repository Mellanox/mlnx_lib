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

#define LIB_COMMU_C_
#define LIB_COMMU_DB_C_

#include "lib_commu.h"
#include "lib_commu_db.h"
#include "lib_commu_bail.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/tcp.h>

#undef  __MODULE__
#define __MODULE__ LIB_COMMU

/************************************************
 *  Global variables
 ***********************************************/

pthread_t listener_threads[NUM_OF_LISTENER_THREADS];
struct listner_exit_event_info listner_exit_event_info_st;
lib_commu_log_cb_t lib_commu_log_cb = NULL;
/************************************************
 *  Local variables
 ***********************************************/

static uint32_t g_lib_commu_init_done = 0;
/*static uint32_t g_lib_commu_verbosity_level = LCOMMU_VERBOSITY_LEVEL_NOTICE;*/
static pthread_mutex_t lock_listener_db_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_tcp_session_db_access =
    PTHREAD_MUTEX_INITIALIZER;
static int client_connect_exit_event_fd = INVALID_HANDLE_ID;
static uint32_t client_connect_thread_cnt = 0;
static enum lib_commu_verbosity_level LOG_VAR_NAME(__MODULE__) =
    LCOMMU_VERBOSITY_LEVEL_NOTICE;

/************************************************
 *  Local function declarations
 ***********************************************/

static int lib_db_init();
static int lib_db_deinit();
static int pseudo_random_gen_uint32_init(void);
static int pseudo_random_uint32_get(uint32_t *magic);

static int comm_lib_udp_ll_send(handle_t handle, struct addr_info recipient_st,
                                uint8_t *buffer, uint32_t *buffer_len);

static int comm_lib_udp_ll_recv(handle_t handle, uint8_t *payload,
                                uint32_t *payload_size,
                                struct addr_info *addresser_st);

static int
comm_lib_tcp_ll_send_blocking(handle_t handle, uint8_t *buffer,
                              uint32_t *buffer_len,
                              enum db_type handle_db_type);

static int
comm_lib_tcp_ll_recv_blocking(handle_t handle, uint8_t *buffer,
                              uint32_t *buffer_len);

static int parse_metadata_from_buffer(uint8_t *payload,
                                      struct msg_metadata *pkt_metadata_st);

static int metadata_set(struct msg_metadata *metadata_st, uint8_t version,
                        uint32_t payload_size,
                        struct handle_info handle_info_st);

static int
validate_metadata_info(struct msg_metadata *metadata_st,
                       uint32_t max_paylod_size,
                       struct handle_info *handle_info_st);

static void * listener_main_thread(void *args);

static void * client_connect_thread(void *args);

static int close_socket_wrapper(handle_t handle);

static int exit_server_thread(uint16_t idx);

static int create_server_event_socket(int *fd, uint16_t idx);

/*
 * This function sets socket options on recv/send buffer of the socekt to
 * MAX_JUMBO_TCP_PAYLOAD.
 *
 * @param[in] - sock_fd - the socket file descriptor
 *
 * @return 0 if operation completes successfully
 * @return errno codes of native setsockopt function
 */
static int set_sock_buffer_size(int sock_fd);

/*
 * This function sets socket options for socket priority to level 6
 *
 * @param[in] - sock_fd - the socket file descriptor
 *
 * @return 0 if operation completes successfully
 * @return errno codes of native setsockopt function
 */
static int set_sock_priority(int sock_fd);

static int handle_new_non_blocking_client(
    int client_fd, int def_flags, struct connect_thread_args *thread_args,
    int *is_sock_sent_client);

static int
create_client_connect_event_socket(int *fd);

static int
exit_client_connect_threads(void);

/************************************************
 *  Local function implementations
 ***********************************************/

static int
lib_db_init()
{
    int err = 0;

    memset(listener_threads, 0, sizeof(listener_threads));
    memset(&listner_exit_event_info_st, 0, sizeof(listner_exit_event_info_st));

    client_connect_thread_cnt = 0;

    err = lib_commu_db_init();
    lib_commu_bail_error(err);

bail:
    return err;
}


static int
lib_db_deinit()
{
    int err = 0;

    memset(listener_threads, 0, sizeof(listener_threads));
    memset(&listner_exit_event_info_st, 0, sizeof(listner_exit_event_info_st));

    client_connect_thread_cnt = 0;

    err = lib_commu_db_deinit();
    lib_commu_bail_error(err);

bail:
    return err;
}


static int
pseudo_random_gen_uint32_init(void)
{
    int err = 0;

    struct timeval t;
    memset(&t, 0, sizeof(t));

    if (gettimeofday(&t, NULL) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Failed gettimeofday() with err[%d]: %s",
                errno, strerror(errno));
        lib_commu_bail_error(errno);
    }

    srand(t.tv_usec * t.tv_sec);

bail:
    return err;
}

static int
pseudo_random_uint32_get(uint32_t *magic)
{
    int err = 0;

    lib_commu_bail_null(magic);
    *magic = INVALID_MAGIC;

    *magic = rand() % UINT32_MAX;

bail:
    LCM_LOG(LCOMMU_LOG_DEBUG, "Create magic [%u]\n", *magic);
    return err;
}


/**
 * * This function is used to close file descriptor (socket)
 *
 * @param[in] - handle - the socket to close
 *
 * @return EBADF or EINVAL - handle isn't a valid
 * @return EINTR - The close() call was interrupted by a signal
 * @return EIO - An I/O error occurred.
 *
 */
static int
close_socket_wrapper(handle_t handle)
{
    int err = 0;

    if (handle == INVALID_HANDLE_ID) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid socket[%d]", handle);
        lib_commu_bail_force(EINVAL);
    }

    if (close(handle) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "close() socket[%d] failed error(%u) :%s",
                handle, errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

static int
comm_lib_udp_ll_send(handle_t handle, struct addr_info recipient_st,
                     uint8_t *buffer, uint32_t *buffer_len)
{
    int err = 0, err_bail = 0;
    struct sockaddr_in recipient;
    int nb_sent = 0;
    enum db_type handle_db_type = UDP_HANDLE_DB;

    memset((char*) &recipient, 0, sizeof(recipient));

    recipient.sin_family = AF_INET;
    recipient.sin_port = recipient_st.port;
    recipient.sin_addr.s_addr = recipient_st.ipv4_addr;

    nb_sent = sendto(handle, buffer, *buffer_len, 0,
                     (struct sockaddr *)&(recipient), sizeof(recipient));

    LCM_LOG(LCOMMU_LOG_DEBUG, "#bytes sent [%d]\n", nb_sent);

    if (nb_sent < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "sendto() failed with err(%d): %s\n", errno,
                strerror(errno));
        *buffer_len = 0;
        lib_commu_bail_error(errno);
    }
    else if ((uint32_t)nb_sent != *buffer_len) {    /* the cast is ok, since nb_recvd >= 0 */
        LCM_LOG(LCOMMU_LOG_ERROR, "Failed to send all message [%d/%u]\n",
                nb_sent, *buffer_len);
        *buffer_len = nb_sent;
        lib_commu_bail_error(EIO);
    }

bail:
    if (nb_sent > 0) {
        err_bail = handle_total_tx_update(handle, &handle_db_type, nb_sent);
        if (err_bail) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed in to update tx[%d] on handle[%d]\n", nb_sent,
                    handle);
            if (err == 0) {
                lib_commu_return_from_bail(err_bail);
            }
        }
    }
    return err;
}


static int
comm_lib_udp_ll_recv(handle_t handle, uint8_t *payload,
                     uint32_t *payload_size, struct addr_info *addresser_st)
{
    int err = 0, err_bail = 0;
    int nb_recvd = 0;
    enum db_type handle_db_type = UDP_HANDLE_DB;
    struct sockaddr_in addresser; /* the sender */
    socklen_t addresser_size = sizeof(addresser);

    memset((char*) &addresser, 0, sizeof(addresser));

    nb_recvd = recvfrom(handle, payload, *payload_size, 0,
                        (struct sockaddr *) &addresser, &addresser_size);

    LCM_LOG(LCOMMU_LOG_DEBUG, "#bytes received: [%d]\n", nb_recvd);

    if (nb_recvd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Failed in recvfrom() with err[%d]: %s",
                errno, strerror(errno));
        *payload_size = 0;
        lib_commu_bail_force(errno);
    }

    *payload_size = nb_recvd;

    addresser_st->ipv4_addr = addresser.sin_addr.s_addr;
    addresser_st->port = addresser.sin_port;

bail:
    if (nb_recvd > 0) {
        err_bail = handle_total_rx_update(handle, &handle_db_type, nb_recvd);
        if (err_bail) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed in to update rx[%d] on handle[%d]\n", nb_recvd,
                    handle);
            if (err == 0) {
                lib_commu_return_from_bail(err_bail);
            }
        }
    }
    return err;
}


static int
comm_lib_tcp_ll_send_blocking(handle_t handle, uint8_t *buffer,
                              uint32_t *buffer_len,
                              enum db_type handle_db_type)
{
    int err = 0, err_bail = 0;
    uint32_t total_bytes = 0; /* how many bytes we've sent */
    int bytes_left = *buffer_len; /* how many we have left to send */
    int nb_sent = 0;
    uint16_t repeat_times = 0;

    while (total_bytes < *buffer_len) {
        if (repeat_times == SEND_REPEAT_NUM) {
            LCM_LOG(LCOMMU_LOG_ERROR, "Retry to send the buffer %d times",
                    repeat_times);
            *buffer_len = total_bytes;
            lib_commu_bail_error(EIO);
        }
        nb_sent = send(handle, (buffer + total_bytes), bytes_left, 0);
        if (nb_sent < 0) {
            LCM_LOG(LCOMMU_LOG_ERROR, "Failed in send() with err[%d]: %s",
                    errno, strerror(errno));
            *buffer_len = 0;
            lib_commu_bail_error(errno);
        }

        total_bytes += nb_sent;
        bytes_left -= nb_sent;
        repeat_times++;
    }

    *buffer_len = total_bytes;

    LCM_LOG(LCOMMU_LOG_DEBUG, "#bytes sent  [%d]\n", total_bytes);

bail:
    if (total_bytes > 0) {
        err_bail =
            handle_total_tx_update(handle, &handle_db_type, total_bytes);
        if (err_bail) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed in to update tx[%d] on handle[%d]\n", total_bytes,
                    handle);
            if (err == 0) {
                lib_commu_return_from_bail(err_bail);
            }
        }
    }
    return err;
}


static int
comm_lib_tcp_ll_recv_blocking(handle_t handle, uint8_t *buffer,
                              uint32_t *buffer_len)
{
    int err = 0, err_bail = 0;
    int n_bytes = 0;
    uint32_t total_bytes = 0;
    uint16_t repeat_times = 0;
    enum db_type handle_db_type = ANY_HANLDE_DB;

    while (total_bytes != *buffer_len) {
        if (repeat_times == RECV_REPEAT_NUM) {
            LCM_LOG(LCOMMU_LOG_ERROR, "Retry to receive the buffer %d times",
                    repeat_times);
            *buffer_len = total_bytes;
            lib_commu_bail_error(EIO);
        }
        n_bytes = recv(handle, (buffer + total_bytes),
                       (*buffer_len - total_bytes), 0);
        if (n_bytes == 0) {
            /*If the remote side has closed the connection, recv() will return 0*/
            LCM_LOG(LCOMMU_LOG_NOTICE, "Peer reset the connection");
            *buffer_len = 0;
            lib_commu_bail_force(ECONNRESET);
        }
        else if (n_bytes == -1) {
            LCM_LOG(LCOMMU_LOG_ERROR, "recv() faild with err(%d): %s", errno, strerror(
                        errno));
            *buffer_len = 0;
            lib_commu_bail_force(errno);
        }

        repeat_times++;
        total_bytes += n_bytes;
    }

    *buffer_len = total_bytes;

    LCM_LOG(LCOMMU_LOG_DEBUG, "#bytes recieved [%d]\n", total_bytes);

bail:
    if (total_bytes > 0) {
        err_bail =
            handle_total_rx_update(handle, &handle_db_type, total_bytes);
        if (err_bail) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed in to update rx[%d] on handle[%d]\n", total_bytes,
                    handle);
            if (err == 0) {
                lib_commu_return_from_bail(err_bail);
            }
        }
    }
    return err;
}

static int
parse_metadata_from_buffer(uint8_t *payload,
                           struct msg_metadata *pkt_metadata_st)
{
    int err = 0;

    lib_commu_bail_null(pkt_metadata_st);

    memcpy(pkt_metadata_st, payload, sizeof(*pkt_metadata_st));

    LCM_LOG(LCOMMU_LOG_INFO,
            "metadata info: msg_type[%u], payload_size[%u], peer_magic[%u], version[%u]\n",
            pkt_metadata_st->msg_type, pkt_metadata_st->payload_size,
            pkt_metadata_st->trailer, pkt_metadata_st->version);

bail:
    return err;
}

static int
metadata_set(struct msg_metadata *metadata_st, uint8_t version,
             uint32_t payload_size, struct handle_info handle_info_st)
{
    int err = 0;

    lib_commu_bail_null(metadata_st);

    metadata_st->version = version;
    metadata_st->payload_size = htonl(payload_size);
    metadata_st->trailer = htonl(handle_info_st.socekt_info.local_magic);
    metadata_st->msg_type = handle_info_st.conn_info.msg_type;

bail:
    return err;
}

static int
validate_metadata_info(struct msg_metadata *metadata_st,
                       uint32_t max_paylod_size,
                       struct handle_info *handle_info_st)
{
    int err = 0;

    metadata_st->payload_size = ntohl(metadata_st->payload_size);
    metadata_st->trailer = ntohl(metadata_st->trailer);

    /* check commu lib msg version */
    if (metadata_st->version != MSG_VERSION) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "comm_lib msg version[%d] != msg version[%u] received\n",
                MSG_VERSION, metadata_st->version);
        lib_commu_bail_force(EIO);
    }

    /* if user payload size <  packet size */
    if ((max_paylod_size < metadata_st->payload_size)
        || (metadata_st->payload_size == 0)) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid payload size [%u]\n",
                metadata_st->payload_size);
        lib_commu_bail_force(EOVERFLOW);
    }

    /* check proper magic */
    if (handle_info_st->socekt_info.peer_magic == INVALID_MAGIC) {
        handle_info_st->socekt_info.peer_magic = metadata_st->trailer;
    }
    else if (handle_info_st->socekt_info.is_single_peer
             && (handle_info_st->socekt_info.peer_magic !=
                 metadata_st->trailer)) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid peer magic [%u]\n",
                metadata_st->trailer);
        lib_commu_bail_force(EBADE);
    }

    /* check proper msg type */
    if (metadata_st->msg_type != handle_info_st->conn_info.msg_type) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Connection default msg_type[%u], msg_type received[%u]\n",
                metadata_st->msg_type, handle_info_st->conn_info.msg_type);
        lib_commu_bail_force(EBADE);
    }


bail:
    return err;
}


static int
set_sock_buffer_size(int sock_fd)
{
    int err = 0;
    int sock_buff_size = 0;

    sock_buff_size = MAX_JUMBO_TCP_PAYLOAD;
    if ((setsockopt(sock_fd, 0, SO_RCVBUF, &sock_buff_size,
                    sizeof(sock_buff_size))) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [SO_RCVBUF], err[%d]: %s\n", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }
    if ((setsockopt(sock_fd, 0, SO_SNDBUF, &sock_buff_size,
                    sizeof(sock_buff_size))) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [SO_SNDBUF], err[%d]: %s\n", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

static int
set_sock_priority(int sock_fd)
{
    int err = 0;
    int priority = 6;
    int iptos_precedence = 0xc0 /*IPTOS_CLASS_CS6*/;

    /* Set SO_PRIORITY for VRRP traffic */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_PRIORITY, &priority,
                   sizeof(priority)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [SO_PRIORITY], err[%d]: %s\n",
                errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    if (setsockopt(sock_fd, IPPROTO_IP, IP_TOS, &iptos_precedence,
                   sizeof(iptos_precedence)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [IP_TOS], err[%d]: %s\n", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

static int
set_sock_ka(int sock_fd)
{
    int err = 0;
    int optval = 1;
    int keepcnt = 3;
    int keepidle = 2;
    int keepintvl = 1;

    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt,
                   sizeof(keepcnt)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [TCP_KEEPCNT], err[%d]: %s\n",
                errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle,
                   sizeof(keepidle)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [TCP_KEEPIDLE], err[%d]: %s\n",
                errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl,
                   sizeof(keepintvl)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [TCP_KEEPINTVL], err[%d]: %s\n",
                errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* Set SO_KEEPALIVE */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &optval,
                   sizeof(optval)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [SO_KEEPALIVE], err[%d]: %s\n",
                errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

static void*
listener_main_thread(void* args)
{
    int err = 0;
    int new_sock = INVALID_HANDLE_ID;
    int fd_max = INVALID_HANDLE_ID;
    int listener_fd = INVALID_HANDLE_ID;
    int i = 0;
    int n_bytes = 0;
    uint16_t server_id_buff = 0;
    uint32_t local_magic = INVALID_MAGIC;
    fd_set read_fds;
    struct sockaddr_in client_addr;
    struct sockaddr_un event_addr; /* when thread receiving event */
    socklen_t event_addr_len = sizeof(event_addr);
    struct listener_thread_args *thread_args = NULL;
    int is_db_locked = 0;
    struct addr_info peer_addr_info;
    struct addr_info local_addr_info;
    int is_sock_sent_client = 0;

/*
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
 */

    memset((char*)&client_addr, 0, sizeof(client_addr));
    memset((char*)&event_addr, 0, sizeof(event_addr));
    memset(&peer_addr_info, 0, sizeof(peer_addr_info));
    memset(&local_addr_info, 0, sizeof(local_addr_info));
    FD_ZERO(&read_fds);

    lib_commu_bail_null(args);

    thread_args = (struct listener_thread_args*)args;
    listener_fd = thread_args->listener_fd;
    if (listener_fd == 0) {
        lib_commu_bail_force(EINVAL);
    }

    local_addr_info.ipv4_addr = thread_args->params.s_ipv4_addr;
    local_addr_info.ipv4_addr = thread_args->params.port;

    if (listen(listener_fd, PENDING_CONNECTIOS_SIZE) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "listen() failed on listener socket[%d] failed with err(%d): %s\n",
                listener_fd, errno, strerror(errno));

        lib_commu_bail_force(errno);
    }

    err = lib_commu_db_server_status_listener_set(thread_args->server_id,
                                                  listener_fd,
                                                  HANDLE_STATUS_UP);
    lib_commu_bail_error(err);

    /* set max fd to select on */
    fd_max = listener_fd > thread_args->event_fd ?
             listener_fd : thread_args->event_fd;

    while (1) {
        socklen_t sin_size = sizeof(client_addr);
        FD_ZERO(&read_fds);
        FD_SET(listener_fd, &read_fds);
        FD_SET(thread_args->event_fd, &read_fds);
        is_sock_sent_client = 0;

        if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno != EINTR) { /* if select caught signal --> don't exit with error */
                LCM_LOG(LCOMMU_LOG_ERROR, "select() failed with err(%d): %s\n",
                        errno, strerror(errno));
                lib_commu_bail_force(errno);
            }
        }

        for (i = 0; i <= fd_max; i++) {
            if (FD_ISSET(i, &read_fds)) { /* socket is read-ready */
                if (i == listener_fd) {
                    /* new connection */
                    new_sock = accept(listener_fd,
                                      (struct sockaddr *) &client_addr,
                                      &sin_size);
                    if (new_sock < 0) {
                        LCM_LOG(LCOMMU_LOG_ERROR,
                                "accept() on listener socket[%d] failed with err(%d): %s\n",
                                listener_fd, errno, strerror(errno));
                        lib_commu_bail_force(errno);
                    }

                    /* enlarge soeckt send/recv buffer to max tcp jumbo size */
                    err = set_sock_buffer_size(new_sock);
                    lib_commu_bail_error(err);

                    /* set priority on socket */
                    err = set_sock_priority(new_sock);
                    lib_commu_bail_error(err);

                    err = set_sock_ka(new_sock);
                    lib_commu_bail_error(err);

                    /*start update DB*/
                    err = pseudo_random_uint32_get(&local_magic);   /* get local magic */
                    lib_commu_bail_error(err);

                    memset(&peer_addr_info, 0, sizeof(peer_addr_info)); /* set peer addr */
                    peer_addr_info.ipv4_addr = client_addr.sin_addr.s_addr;
                    peer_addr_info.port = client_addr.sin_port;

                    err = pthread_mutex_lock(&lock_listener_db_access);
                    lib_commu_bail_error(err);
                    is_db_locked = 1;

                    err = lib_commu_db_tcp_server_handle_info_set(
                        thread_args->server_id, new_sock,
                        thread_args->params.msg_type, local_addr_info,
                        peer_addr_info, local_magic);
                    lib_commu_bail_error(err);

                    err = lib_cummo_db_server_status_client_set(
                        new_sock, &client_addr,
                        thread_args->params.s_ipv4_addr,
                        thread_args->params.port,
                        thread_args->server_id);
                    lib_commu_bail_error(err);

                    err = pthread_mutex_unlock(&lock_listener_db_access);
                    lib_commu_bail_error(err);
                    is_db_locked = 0;
                    /*finish update DB*/

                    /*send new socket to client*/
                    err = thread_args->clbk_st.clbk_notify_func(
                        new_sock, peer_addr_info,
                        thread_args->clbk_st.data, 0);
                    lib_commu_bail_error(err);

                    is_sock_sent_client = 1;
                }
                /* Receiving  event */
                else if (i == thread_args->event_fd) {
                    n_bytes = recvfrom(i, &server_id_buff,
                                       sizeof(server_id_buff), 0,
                                       (struct sockaddr*) &event_addr,
                                       &event_addr_len);
                    if (n_bytes < 0) {
                        LCM_LOG(LCOMMU_LOG_ERROR,
                                "Fail to receive data on exit event FD, err(%d): %s",
                                errno, strerror(errno));
                        lib_commu_bail_force(errno);
                    }

                    if (thread_args->server_id == server_id_buff) {
                        err = close_socket_wrapper(listener_fd);
                        lib_commu_bail_error(err);
                        err = close_socket_wrapper(thread_args->event_fd);
                        lib_commu_bail_error(err);
                        LCM_LOG(LCOMMU_LOG_NOTICE,
                                "Received exit event on server id[%d]",
                                thread_args->server_id);
                        goto bail;
                    }
                    /* programming error */
                    else {
                        LCM_LOG(LCOMMU_LOG_WARNING,
                                "got server_id[%d], thread_id[%lu], db_server_id[%u]\n",
                                server_id_buff, pthread_self(),
                                thread_args->server_id);
                    }
                } /*end receiving event*/
            }
        } /*end for*/
    } /*end while*/

bail:
    if (is_db_locked) {
        pthread_mutex_unlock(&lock_listener_db_access);
    }
    LCM_LOG(LCOMMU_LOG_NOTICE,
            "Exit from listener thread, server id[%u] with err(%d)\n",
            thread_args->server_id, err);
    if (err && !is_sock_sent_client && (new_sock != INVALID_HANDLE_ID)) {
        close_socket_wrapper(new_sock);
    }
    safe_free(args);
    pthread_exit(&err);
}


static int
create_server_event_socket(int *fd, uint16_t idx)
{
    int err = 0;
    int len = 0;
    struct sockaddr_un s_addr;

    memset(&s_addr, 0, sizeof(s_addr));

    lib_commu_bail_null(fd);

    *fd = 0;

    /* Create socket from which to read. */
    *fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (*fd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Could not create socket err(%d) :%s\n",
                errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    s_addr.sun_family = AF_UNIX;
    strcpy(s_addr.sun_path, listner_exit_event_info_st.event_socket_path[idx]);
    unlink(s_addr.sun_path);

    len = strlen(s_addr.sun_path) + sizeof(s_addr.sun_family);
    if (bind(*fd, (struct sockaddr *)&s_addr, len) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Failed to bind socket[%d] err(%d): %s\n",
                *fd, errno, strerror(
                    errno));
        lib_commu_bail_force(errno);
    }

    listner_exit_event_info_st.event_fd[idx] = *fd;


bail:
    return err;
}

static int
exit_server_thread(uint16_t idx)
{
    int err = 0;
    int n_bytes = 0;
    int send_event_fd = INVALID_HANDLE_ID;
    struct sockaddr_un s_addr;
    socklen_t sock_len = 0;

    memset(&s_addr, 0, sizeof(s_addr));

    send_event_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (send_event_fd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Could not create socket err(%d) :%s\n",
                errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    s_addr.sun_family = AF_UNIX;
    strcpy(s_addr.sun_path, listner_exit_event_info_st.event_socket_path[idx]);
    sock_len = strlen(s_addr.sun_path) + sizeof(s_addr.sun_family);

    n_bytes = sendto(send_event_fd, &idx, sizeof(idx), 0,
                     (const struct sockaddr*) &s_addr, sock_len);
    if (n_bytes < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Writing to socket[%d] on path: %s, failed with err(%d): %s\n",
                listner_exit_event_info_st.event_fd[idx], s_addr.sun_path,
                errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    listner_exit_event_info_st.event_fd[idx] = INVALID_HANDLE_ID;


bail:
    close_socket_wrapper(send_event_fd);
    return err;
}


static void*
client_connect_thread(void *args)
{
    int err = 0, bail_err = 0;
    int so_err_val = 0;
    uint32_t so_err_val_len = sizeof(so_err_val);
    struct addr_info peer_info;
    int client_fd = INVALID_HANDLE_ID;
    int is_sock_sent_client = 0;
    int fd_max = INVALID_HANDLE_ID;
    int fd_flags = 0;
    int i = 0;
    struct sockaddr_in server_sock_addr;
    socklen_t sock_addr_len = 0;
    struct connect_thread_args *thread_args = NULL;
    fd_set write_fds;
    fd_set read_fds;

    client_connect_thread_cnt++;

    memset(&server_sock_addr, 0, sizeof(server_sock_addr));
    memset(&peer_info, 0, sizeof(peer_info));
    FD_ZERO(&write_fds);
    FD_ZERO(&read_fds);

    lib_commu_bail_null(args);

    thread_args = (struct connect_thread_args*)args;
    client_fd = thread_args->client_fd;
    if (client_fd == 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid client FD");
        lib_commu_bail_force(EINVAL);
    }

    /* set socket as non-blocking */
    if ((fd_flags = fcntl(client_fd, F_GETFL, 0)) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "fcntl failed: on socket[%d] err(%u): %s",
                client_fd, errno, strerror(errno));
        lib_commu_bail_force(errno);
    }
    if (fcntl(client_fd, F_SETFL, fd_flags | O_NONBLOCK) < 0) {
        close_socket_wrapper(client_fd);
        LCM_LOG(LCOMMU_LOG_ERROR, "fcntl failed: on socket[%d] err(%u): %s",
                client_fd, errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* prepare server params to connect */
    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_port = thread_args->conn_info.d_port;
    server_sock_addr.sin_addr.s_addr = thread_args->conn_info.d_ipv4_addr;
    sock_addr_len = sizeof(struct sockaddr);

    /* connect non-blocking mode*/
    err = connect(client_fd, (struct sockaddr*) &server_sock_addr,
                  sock_addr_len);
    if (err < 0) {
        if (errno != EINPROGRESS) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "None blocking connect failed: on socket[%d] err(%u): %s",
                    client_fd, errno, strerror(errno));
            lib_commu_bail_force(errno);
        }
    }
    else { /* connect is already done */
        err = handle_new_non_blocking_client(client_fd, fd_flags, thread_args,
                                             &is_sock_sent_client);
        lib_commu_bail_error(err);
        goto bail;
    }

    fd_max =
        (client_fd > client_connect_exit_event_fd) ?
        client_fd : client_connect_exit_event_fd;

    while (1) {
        /* set args for select */
        FD_ZERO(&write_fds);
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &write_fds);
        FD_SET(client_connect_exit_event_fd, &read_fds);

        /* wait on socket with select */
        if (select(fd_max + 1, &read_fds, &write_fds, NULL,
                   thread_args->use_tval ? &(thread_args->tval) : NULL) < 0) {
            if (errno != EINTR) { /* if select caught signal --> don't exit with error */
                LCM_LOG(LCOMMU_LOG_ERROR, "select() failed with err(%d): %s\n",
                        errno, strerror(errno));
                lib_commu_bail_force(errno);
            }
        }

        for (i = 0; i <= fd_max; i++) {
            is_sock_sent_client = 0;
            if (FD_ISSET(i, &write_fds)) { /* socket is write-ready */
                if (i == client_fd) {
                    if (getsockopt(i, SOL_SOCKET, SO_ERROR, &so_err_val,
                                   &so_err_val_len)
                        < 0) {
                        LCM_LOG(LCOMMU_LOG_ERROR,
                                "getsockopt failed with err(%d): %s\n", errno,
                                strerror(errno));
                        lib_commu_bail_force(errno);
                    }
                    if (so_err_val == 0) { /* connection established */
                        err = handle_new_non_blocking_client(
                            client_fd, fd_flags, thread_args,
                            &is_sock_sent_client);
                        lib_commu_bail_error(err);
                        goto bail;
                    }
                    else {
                        LCM_LOG(LCOMMU_LOG_ERROR,
                                "Failed to establish connection with err(%d): %s\n",
                                so_err_val, strerror(so_err_val));
                        lib_commu_bail_force(so_err_val);
                    }
                }
            }
            else if (FD_ISSET(i, &read_fds)) { /* socket is write-ready */
                if (i == client_connect_exit_event_fd) {
                    LCM_LOG(LCOMMU_LOG_NOTICE,
                            "Exiting from client connect thread");
                    goto bail;
                }
            }
        } /* end for loop on FDs */
    } /* end while */

bail:
    if (err && (client_fd != INVALID_HANDLE_ID)) {
        /* handle failure */
        if (!is_sock_sent_client) {
            close_socket_wrapper(client_fd);
        }
        peer_info.ipv4_addr = thread_args->conn_info.d_ipv4_addr;
        peer_info.port = thread_args->conn_info.d_port;
        err = -err;
        bail_err = thread_args->clbk_st.clbk_notify_func(INVALID_HANDLE_ID,
                                                         peer_info,
                                                         thread_args->clbk_st.data,
                                                         err);
        if (bail_err) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed to notify client on new handle with err(%d)\n",
                    bail_err);
        }
    }
    client_connect_thread_cnt--;
    safe_free(args);
    pthread_exit(NULL);
}


static int
handle_new_non_blocking_client(
    int client_fd, int def_flags, struct connect_thread_args *thread_args,
    int *is_sock_sent_client)
{
    int err = 0;
    uint32_t local_magic = 0;
    struct addr_info peer_info;
    struct addr_info local_info;

    memset(&local_info, 0, sizeof(local_info));
    memset(&peer_info, 0, sizeof(peer_info));

    lib_commu_bail_null(thread_args);

    if (client_fd == INVALID_HANDLE_ID) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid client FD\n");
        lib_commu_bail_force(EPERM);
    }

    /* restore file status flags - set as blocking */
    if (fcntl(client_fd, F_SETFL, def_flags) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "fcntl failed: on socket[%d] err(%u): %s",
                client_fd, errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* set local magic for the tcp client session */
    err = pseudo_random_uint32_get(&local_magic);
    lib_commu_bail_error(err);

    peer_info.ipv4_addr = thread_args->conn_info.d_ipv4_addr;
    peer_info.port = thread_args->conn_info.d_port;

    /* insert handle to DB */
    err = lib_commu_db_tcp_client_handle_info_set(client_fd,
                                                  thread_args->conn_info.msg_type,
                                                  local_info, peer_info,
                                                  local_magic);
    lib_commu_bail_error(err);

    /*send new socket to client*/
    err = thread_args->clbk_st.clbk_notify_func(client_fd, peer_info,
                                                thread_args->clbk_st.data, 0);
    lib_commu_bail_error(err);

    *is_sock_sent_client = 1;

    LCM_LOG(LCOMMU_LOG_INFO, "Establish connection on socket[%d]\n",
            client_fd);

bail:
    return err;
}


static int
create_client_connect_event_socket(int *fd)
{
    int err = 0;
    int len = 0;
    struct sockaddr_un s_addr;

    memset(&s_addr, 0, sizeof(s_addr));

    lib_commu_bail_null(fd);

    *fd = 0;

    /* Create socket from which to read. */
    *fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (*fd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Could not create connect event socket err(%d): %s\n", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    s_addr.sun_family = AF_UNIX;
    strcpy(s_addr.sun_path, CLIENT_CONNECT_THREAD_PATH);
    unlink(s_addr.sun_path);

    len = strlen(s_addr.sun_path) + sizeof(s_addr.sun_family);
    if (bind(*fd, (struct sockaddr *)&s_addr, len) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Failed to bind socket[%d] err(%d): %s\n",
                *fd, errno, strerror( errno));
        lib_commu_bail_force(errno);
    }

bail:
    return err;
}

static int
exit_client_connect_threads(void)
{
    int err = 0;
    int n_bytes = 0;
    int send_event_fd = INVALID_HANDLE_ID;
    struct sockaddr_un s_addr;
    socklen_t sock_len = 0;
    int exit_code = 1;

    memset(&s_addr, 0, sizeof(s_addr));

    send_event_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (send_event_fd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Could not create exit client connect event socket err(%d): %s\n",
                errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    s_addr.sun_family = AF_UNIX;
    strcpy(s_addr.sun_path, CLIENT_CONNECT_THREAD_PATH);
    sock_len = strlen(s_addr.sun_path) + sizeof(s_addr.sun_family);

    n_bytes = sendto(send_event_fd, &exit_code, sizeof(exit_code), 0,
                     (const struct sockaddr*) &s_addr, sock_len);
    if (n_bytes < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Writing to socket[%d] on path: %s, failed with err(%d): %s\n",
                send_event_fd, s_addr.sun_path, errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

bail:
    close_socket_wrapper(send_event_fd);
    return err;
}

/************************************************
 *  Function implementations
 ***********************************************/


/**
 * This function is used to open communication library and init it's data
 *
 * @param[in] - log_cb_t logging_cb
 * @param[in, out] - None
 * @param[out] - None
 *
 * @return 0 if operation completes successfully
 * @return EFAULT or EINVAL or EPERM if pseusdo random init failed
 * @return EPERM if DB operation failed
 * @return errno codes of native pthread_mutex_init function
 */
int
comm_lib_init(lib_commu_log_cb_t logging_cb)
{
    int err = 0;

    if (logging_cb == NULL) {
        /* do nothing */
    }
    else {
        lib_commu_log_cb = logging_cb;
    }

    if (g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_NOTICE, "Library already initialized\n");
        goto bail;
    }

    err = lib_db_init();
    lib_commu_bail_error(err);

    /* Initialize MUTEX */
    if (pthread_mutex_init(&lock_listener_db_access, NULL) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "pthread_mutex_init() failed with err(%d): %s\n", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }
    if (pthread_mutex_init(&lock_tcp_session_db_access, NULL) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "pthread_mutex_init() failed with err(%d): %s\n", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    err = pseudo_random_gen_uint32_init();
    lib_commu_bail_error(err);

    err = create_client_connect_event_socket(&client_connect_exit_event_fd);
    lib_commu_bail_error(err);

    g_lib_commu_init_done = 1;
    LCM_LOG(LCOMMU_LOG_NOTICE,
            "Communication library finish initialization\n");

bail:
    return -err;
}



/**
 * This function is used to close communication library and deinit it's data
 *
 * @param[in] - None
 * @param[in, out] - None
 * @param[out] - None
 *
 * @return 0 if operation completes successfully
 * @return EPERM if library didn't finish init
 * @return errno codes of native pthread_mutex_destroy function
 */
int
comm_lib_deinit()
{
    int err = 0, tmp_err = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    g_lib_commu_init_done = 0;

    tmp_err = pthread_mutex_destroy(&lock_listener_db_access);
    if (tmp_err != 0) {
        err = tmp_err;
        LCM_LOG(LCOMMU_LOG_ERROR,
                "pthread_mutex_destroy() failed with err(%d): %s\n", errno,
                strerror(errno));
    }
    tmp_err = pthread_mutex_destroy(&lock_tcp_session_db_access);
    if (tmp_err != 0) {
        err = tmp_err;
        LCM_LOG(LCOMMU_LOG_ERROR,
                "pthread_mutex_destroy() failed with err(%d): %s\n", errno,
                strerror(errno));
    }

    if (client_connect_thread_cnt > 0) {
        tmp_err = exit_client_connect_threads();
        if (tmp_err != 0) {
            err = tmp_err;
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed on to send exit event to client connect thread err(%u)\n",
                    err);
            /*update error and continue, best effort deinit*/
        }
    }

    while (1) {
        if (client_connect_thread_cnt == 0) {
            break;
        }
        sleep(1);
    }

    if (client_connect_exit_event_fd != INVALID_HANDLE_ID) {
        tmp_err = close_socket_wrapper(client_connect_exit_event_fd);
        if (tmp_err != 0) {
            err = tmp_err;
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed to close socketsocket(%u): with err[%u]\n",
                    client_connect_exit_event_fd, err);
            /*update error and continue, best effort deinit*/
        }
    }

    tmp_err = lib_db_deinit();
    lib_commu_bail_error(tmp_err);

bail:
    LCM_LOG(LCOMMU_LOG_NOTICE,
            "Communication library finish deinit with err(%d)\n", err);
    return -err;
}


/**
 * Sets verbosity level of communication library
 *
 * @param[in] - verbosity - verbosity level
 * @param[in, out] - None
 * @param[out] - None
 *
 * @return 0 if operation completes successfully
 * @return EINVAL - Invalid argument - param out of range, if operation completes unsuccessfully
 * @return EPERM if library didn't finish init
 */
int
comm_lib_verbosity_level_set(enum lib_commu_verbosity_level verbosity)
{
    int err = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

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

    err = comm_lib_db_verbosity_level_set(verbosity);
    lib_commu_bail_error(err);


bail:
    return -err;
}


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
comm_lib_verbosity_level_get(enum lib_commu_verbosity_level *verbosity)
{
    int err = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Library isn't initialized\n");
        lib_commu_bail_force(EPERM);
    }

    if (verbosity == NULL) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Param verbosity is NULL\n");
        lib_commu_bail_force(EINVAL);
    }

    *verbosity = LOG_VAR_NAME(__MODULE__);


bail:
    return -err;
}


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
 * @return ENOBUFS or EPERM if can't update session db with the new handle.
 * @return EPERM if library didn't finish init
 */
int
comm_lib_udp_session_start(handle_t *handle,
                           const struct udp_params const *params)
{
    int err = 0;

    int sockfd = INVALID_HANDLE_ID;
    struct sockaddr_in sock_addr;
    uint32_t local_magic = 0;
    socklen_t sock_addr_len = 0;

    memset((char *) &sock_addr, 0, sizeof(sock_addr));

    lib_commu_bail_null(handle);
    lib_commu_bail_null(params);

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    *handle = 0;

    /*
     * AF_INET - IPv4 Internet protocols.
     * SOCK_DGRAM - Supports datagrams.
     * SOCK_NONBLOCK -
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Socket creation failed failed err(%u): %s",
                errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* set priority on socket */
    err = set_sock_priority(sockfd);
    lib_commu_bail_error(err);

    switch (params->type) {
    case CONN_TYPE_UDP_IP_UC:
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_addr.s_addr =
            params->fields.ip_udp_params.session_params.s_ipv4_addr;
        sock_addr.sin_port =
            params->fields.ip_udp_params.session_params.port;
        break;

    default:
        lib_commu_bail_force(EKEYREJECTED);
    }

    /* if connection role is server --> bind the socket  */
    if (params->fields.ip_udp_params.connection_role == CONN_SERVER) {
        sock_addr_len = sizeof(sock_addr);
        if (bind(sockfd, (struct sockaddr *) &sock_addr, sock_addr_len) < 0) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "bind() socket[%d] failed with err(%u): %s\n",
                    sockfd, errno, strerror(errno));
            lib_commu_bail_force(errno);
        }
    }

    /* set local magic for the udp session */
    err = pseudo_random_uint32_get(&local_magic);
    lib_commu_bail_error(err);

    /* insert handle to DB */
    err =
        lib_commu_db_udp_handle_info_set(sockfd,
                                         &(params->fields.ip_udp_params),
                                         local_magic);
    lib_commu_bail_error(err);

    /* update socket fd */
    *handle = sockfd;

bail:
    if (err && (sockfd != INVALID_HANDLE_ID)) {
        close_socket_wrapper(sockfd);
    }
    return -err;
}



/**
 * Close a socket connection
 *
 * @param[in] handle - the handle to close connection.
 * @param[in,out] None.
 * @param[out] None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return 0 if operation completes successfully
 * @return EBADF if handle isn't a valid open file descriptor.
 * @return EINTR if The close() call was interrupted by a signal
 * @return EIO if An I/O error occurred.
 * @return ENOKEY if handle not found in DB
 * @return EPREM if handle == INVALID_HANDLE_ID
 * @return EPERM if library didn't finish init
 */
int
comm_lib_udp_session_stop(handle_t handle)
{
    int err = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    err = close_socket_wrapper(handle);
    lib_commu_bail_error(err);

    err = lib_commu_db_udp_handle_info_delete(handle);
    lib_commu_bail_error(err);


bail:
    return -err;
}


/**
 * Send payload over a UDP connection
 *
 * @param[in] handle - the handle to send data.
 * @param[in] payload - data to send.
 * @param[in] conn_info - the information of the recipient and msg_type sent on handle.
 * @param[in,out] payload_len - sizeof the payload, filled with actual #bytes sent.
 * @param[out] - None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return 0 if operation completes successfully
 * @return EOVERFLOW if payload_len > MAX_UDP_MSG_SIZE (1436) -sizeof(struct msg_metadata)
 * @return ENOKEY if handle wasn't found.
 * @return EIO if failed to send all payload
 * @return EPERM if library didn't finish init
 * @return sendto errno codes if failed to send payload
 */
int
comm_lib_udp_send(handle_t handle, struct addr_info recipient_st,
                  uint8_t *payload, uint32_t *payload_len)
{
    int err = 0;

    struct msg_metadata metadata_st;
    struct handle_info *handle_info_st = NULL;
    uint8_t buffer[MAX_MTU];
    uint32_t buffer_len = 0;
    enum db_type handle_db_type = UDP_HANDLE_DB;

    memset((char*) &metadata_st, 0, sizeof(metadata_st));
    memset((uint8_t*) buffer, 0, sizeof(buffer));

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(payload);
    lib_commu_bail_null(payload_len);
    if (*payload_len > MAX_UDP_MSG_SIZE - sizeof(struct msg_metadata)) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid payload size [%u]\n", *payload_len);
        lib_commu_bail_force(EOVERFLOW);
    }

    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st,
                                       &handle_db_type,
                                       NULL);
    lib_commu_bail_error(err);

    err =
        metadata_set(&metadata_st, MSG_VERSION, *payload_len, *handle_info_st);
    lib_commu_bail_error(err);

    memcpy(buffer, &metadata_st, sizeof(metadata_st));
    buffer_len = sizeof(metadata_st);
    memcpy(buffer + buffer_len, payload, *payload_len);
    buffer_len += *payload_len;

    /* sending the payload */
    err = comm_lib_udp_ll_send(handle, recipient_st, buffer, &buffer_len);
    if (err != 0) {
        if (buffer_len > sizeof(metadata_st)) {
            *payload_len = buffer_len - sizeof(metadata_st);
        }
        else {
            *payload_len = 0;
        }
        lib_commu_bail_force(EIO);
    }

bail:
    return -err;
}



/**
 * Receive payload over a UDP connection,
 * if receive fails the handle is closed and becomes obsolete
 *
 * @param[in] handle - the handle to receive the data
 * @param[in] payload - data received.
 * @param[in,out] payload_len - sizeof the payload, filled with actual #bytes received.
 * @param[out] None.
 *
 * Global Variables Referred & Modified: None
 *
 * @return 0 if operation completes successfully
 * @return EOVERFLOW if payload_len is insufficient.
 * @return EIO if failed to receive any part of the message
 * @return EBADE if got data from unhallowed peer or corrupted data
 * @return EINVAL if payload_len == 0
 * @return EPERM if library didn't finish init
 */

int
comm_lib_udp_recv(handle_t handle, struct addr_info *addresser_st,
                  uint8_t *payload, uint32_t *payload_len)
{
    int err = 0;

    struct msg_metadata metadata_st;
    struct handle_info *handle_info_st = NULL;
    uint8_t buffer[MAX_MTU];
    uint32_t buffer_len = MAX_MTU;
    enum db_type handle_db_type = UDP_HANDLE_DB;

    memset((char*) &metadata_st, 0, sizeof(metadata_st));
    memset((uint8_t*) buffer, 0, sizeof(buffer));

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(payload);
    lib_commu_bail_null(payload_len);
    lib_commu_bail_null(addresser_st);

    if (*payload_len == 0) {
        lib_commu_bail_force(EINVAL);
    }

    /* 0. get handle info */
    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st,
                                       &handle_db_type,
                                       NULL);
    lib_commu_bail_error(err);

    /* 1. get the payload and update the addresser_st info*/
    err = comm_lib_udp_ll_recv(handle, buffer, &buffer_len, addresser_st);
    lib_commu_bail_error(err);

    if (buffer_len < sizeof(metadata_st)) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Data received < metadata\n");
        lib_commu_bail_force(EIO);
    }

    /* 2. get metadata from the payload */
    err = parse_metadata_from_buffer(buffer, &metadata_st);
    lib_commu_bail_error(err);

    err = validate_metadata_info(&metadata_st, *payload_len, handle_info_st);
    lib_commu_bail_error(err);

    /* 4. copy the payload from buffer */
    memcpy(payload, buffer + sizeof(metadata_st), metadata_st.payload_size);
    *payload_len = metadata_st.payload_size;


bail:
    LCM_LOG(LCOMMU_LOG_DEBUG, "finish: %s, with error code[%d]\n", __func__,
            err);
    if (err) {
        *payload_len = 0;
    }
    return -err;
}



/**
 * start a TCP connection from the server side.
 * opens a thread which "listens" to new connections.
 * Once connection is established a callback is being made with
 * a new handle toward the client.
 *
 * @param[in] params - server address, port, etc (network order)
 * @param[in] function callback
 * @param[in,out] server_id - the id of the server to open
 * @param[out] handle
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
                                  uint16_t *server_id)
{
    int err = 0;
    int listener = INVALID_HANDLE_ID;
    int event_fd = INVALID_HANDLE_ID;
    struct sockaddr_in serveraddr;
    int optval = 0;
    socklen_t sockaddr_len = 0;
    struct listener_thread_args *args = NULL;
    uint16_t idx = NUM_OF_LISTENER_THREADS;
    int is_db_locked = 0;

    memset((char *) &serveraddr, 0, sizeof(serveraddr));

    lib_commu_bail_null(clbk_st);
    lib_commu_bail_null(clbk_st->clbk_notify_func);
    lib_commu_bail_null(server_id);

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listener) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Can't create socket, err(%d): %s\n",
                errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = params.port;
    serveraddr.sin_addr.s_addr = params.s_ipv4_addr;

    /*binding the listener socket*/
    optval = 1; /* Set the option active */
    if (-1 == setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval,
                         sizeof(optval))) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Fail setting sock options [SO_REUSEADDR], err[%d]: %s\n",
                errno,
                strerror(errno));
        close_socket_wrapper(listener);
        lib_commu_bail_force(errno);
    }

    sockaddr_len = sizeof(serveraddr);
    if (bind(listener, (struct sockaddr *) &serveraddr, sockaddr_len) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Bind listener socket[%d] failed err(%u): %s\n",
                listener, errno, strerror(errno));
        close_socket_wrapper(listener);
        lib_commu_bail_force(errno);
    }

    /*start take idx from DB*/
    err = pthread_mutex_lock(&lock_tcp_session_db_access);
    lib_commu_bail_error(err);
    is_db_locked = 1;

    err = lib_commu_db_unoccupied_listener_thread_get(&idx);
    if (err) {
        close_socket_wrapper(listener);
        lib_commu_bail_force(ENOMEM);
    }

    *server_id = idx;

    err = create_server_event_socket(&event_fd, idx);
    if (err) {
        close_socket_wrapper(listener);
        lib_commu_bail_error(err);
    }

    args = (struct listener_thread_args *) malloc(
        sizeof(struct listener_thread_args));
    lib_commu_bail_null(args);

    args->event_fd = event_fd;
    args->server_id = idx;
    args->thread_idx = idx;
    args->listener_fd = listener;
    args->clbk_st.clbk_notify_func = clbk_st->clbk_notify_func;
    args->clbk_st.data = clbk_st->data;
    args->params.msg_type = params.msg_type;
    args->params.port = params.port;
    args->params.s_ipv4_addr = params.s_ipv4_addr;

    err = pthread_create(&listener_threads[idx], NULL, listener_main_thread,
                         args);
    if (err != 0) {
        close_socket_wrapper(listener);
        safe_free(args);
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Failed to create listener thread err(%u):\n", err);
        lib_commu_bail_force(err);
    }

    err = pthread_mutex_unlock(&lock_tcp_session_db_access);
    lib_commu_bail_error(err);
    is_db_locked = 0;
    /*finish take idx from DB && assign thread id in listener_threads*/


bail:
    if (is_db_locked) {
        err = pthread_mutex_unlock(&lock_tcp_session_db_access);
        lib_commu_return_from_bail(-err);
    }
    return -err;
}


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
                                 uint32_t handle_array_len)
{
    int err = 0;
    uint32_t i = 0, j = 0;
    uint16_t idx = 0;
    uint16_t thread_idx = 0;
    struct server_status const *server_status = NULL;
    int is_db_locked = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(handle_array);
    if (handle_array_len == 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid parameter [handle_array_len]\n");
        lib_commu_bail_force(EINVAL);
    }

    if (server_id >= SERVER_STATUS_ARR_LENGTH) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid parameter [server_id]\n");
        lib_commu_bail_force(EINVAL);
    }

    idx = server_id;
    thread_idx = server_id;

    err = lib_commu_db_tcp_server_status_get(&server_status, server_id);
    lib_commu_bail_error(err);

    if (server_status->listener_status == HANDLE_STATUS_DOWN) {
        LCM_LOG(LCOMMU_LOG_NOTICE, "Server[%u] already not active\n",
                server_id);
        goto bail;
    }

    /* signal to listener thread to exit */
    err = exit_server_thread(idx);
    lib_commu_bail_error(err);

    LCM_LOG(LCOMMU_LOG_INFO, "Thread id to close[%lu]",
            listener_threads[thread_idx]);
    err = pthread_join(listener_threads[thread_idx], NULL);
    lib_commu_bail_error(err);

    /*start delete session resource DB and sockets*/
    err = pthread_mutex_lock(&lock_tcp_session_db_access);
    lib_commu_bail_error(err);
    is_db_locked = 1;

    listener_threads[thread_idx] = 0;
    LCM_LOG(LCOMMU_LOG_INFO, "listener thread exited\n");

    /* close all clients sockets */
    for (i = 0; i < MAX_CONNECTION_NUM; i++) {
        if (server_status->client_status[i].handle != INVALID_HANDLE_ID) {
            err = close_socket_wrapper(server_status->client_status[i].handle);
            lib_commu_bail_error(err);

            /* best effort */
            if (j < handle_array_len) {
                handle_array[j++] = server_status->client_status[i].handle;
            }
        }
    }

    /* reset TCP DB */
    err = lib_commu_db_tcp_session_db_deinit(server_id);
    lib_commu_bail_error(err);

    err = pthread_mutex_unlock(&lock_tcp_session_db_access);
    lib_commu_bail_error(err);
    is_db_locked = 0;

bail:
    if (is_db_locked) {
        err = pthread_mutex_unlock(&lock_tcp_session_db_access);
        lib_commu_return_from_bail(-err);
    }
    return -err;
}



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
    const struct connection_info const *conn_info,
    handle_t *client_handle)
{
    int err = 0;
    int sock_fd = INVALID_HANDLE_ID;
    struct sockaddr_in server_sock_addr;
    uint32_t local_magic = 0;
    socklen_t sock_addr_len = 0;
    struct addr_info peer_info;
    struct addr_info local_info;

    memset(&server_sock_addr, 0, sizeof(server_sock_addr));
    memset(&local_info, 0, sizeof(local_info));
    memset(&peer_info, 0, sizeof(peer_info));

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(client_handle);
    lib_commu_bail_null(conn_info);

    *client_handle = 0;

    /*
     * AF_INET - IPv4 Internet protocols.
     * SOCK_STREAM - Supports TCP.
     * SOCK_NONBLOCK -
     */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Socket creation failed err(%u): %s", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* enlarge soeckt send/recv buffer to max tcp jumbo size */
    err = set_sock_buffer_size(sock_fd);
    lib_commu_bail_error(err);

    /* set priority on socket */
    err = set_sock_priority(sock_fd);
    lib_commu_bail_error(err);

    /* set keepalive on socket */
    err = set_sock_ka(sock_fd);
    lib_commu_bail_error(err);

    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_port = conn_info->d_port;
    server_sock_addr.sin_addr.s_addr = conn_info->d_ipv4_addr;
    sock_addr_len = sizeof(struct sockaddr);

    if (connect(sock_fd, (struct sockaddr*) &server_sock_addr,
                sock_addr_len) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Connect() failed: on socket[%d] err(%u): %s",
                sock_fd, errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* set local magic for the tcp client session */
    err = pseudo_random_uint32_get(&local_magic);
    lib_commu_bail_error(err);

    peer_info.ipv4_addr = conn_info->d_ipv4_addr;
    peer_info.port = conn_info->d_port;

    /* insert handle to DB */
    err = lib_commu_db_tcp_client_handle_info_set(sock_fd, conn_info->msg_type,
                                                  local_info, peer_info,
                                                  local_magic);
    lib_commu_bail_error(err);

    *client_handle = sock_fd;

bail:
    if (err && (sock_fd != INVALID_HANDLE_ID)) {
        close_socket_wrapper(sock_fd);
    }
    return -err;
}

/**
 * start a TCP connection from the client side. (NON-Blocking)
 * opens a thread which opens a socket toward specific server IP and port
 * Once connection is established, a callback is being made with a new handle
 * toward the client.
 * If connection fails, a callback is being made with handle = -1 and in rc the proper (int*)error code
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
 * @return errno codes of native socket function
 * @return errno codes of native connect function
 */
int
comm_lib_tcp_client_non_blocking_start(
    const struct connection_info const *conn_info,
    struct register_to_new_handle *clbk_st, struct timeval *tval)
{
    int err = 0, err2 = 0;
    int sock_fd = INVALID_HANDLE_ID;
    pthread_t connect_thread = 0;
    pthread_attr_t attr;
    int is_attr_init = 0;
    struct connect_thread_args *args = NULL;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(conn_info);
    lib_commu_bail_null(clbk_st);

    /*
     * AF_INET - IPv4 Internet protocols.
     * SOCK_STREAM - Supports TCP.
     * SOCK_NONBLOCK -
     */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Socket creation failed err(%u): %s", errno,
                strerror(errno));
        lib_commu_bail_force(errno);
    }

    /* enlarge soeckt send/recv buffer to max tcp jumbo size */
    err = set_sock_buffer_size(sock_fd);
    lib_commu_bail_error(err);

    /* set priority on socket */
    err = set_sock_priority(sock_fd);
    lib_commu_bail_error(err);

    args = (struct connect_thread_args *) malloc(
        sizeof(struct connect_thread_args));
    lib_commu_bail_null(args);

    args->client_fd = sock_fd;
    memcpy(&(args->conn_info), conn_info, sizeof(*conn_info));
    args->clbk_st.clbk_notify_func = clbk_st->clbk_notify_func;
    args->clbk_st.data = clbk_st->data;
    args->use_tval = 0;
    if (tval != NULL) {
        memcpy(&(args->tval), tval, sizeof(*tval));
        args->use_tval = 1;
    }

    /* set thread attributs */
    err = pthread_attr_init(&attr);
    if (err != 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Failed to init ptheard attribute(%u):\n", err);
        safe_free(args);
        lib_commu_bail_force(err);
    }
    is_attr_init = 1;

    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (err != 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Failed to set detach state on attribute attribute(%u):\n",
                err);
        safe_free(args);
        lib_commu_bail_force(err);
    }

    err = pthread_create(&connect_thread, &attr, client_connect_thread, args);
    if (err != 0) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Failed to create client connect thread err(%u):\n", err);
        safe_free(args);
        lib_commu_bail_force(err);
    }

bail:
    if (err && (sock_fd != INVALID_HANDLE_ID)) {
        close_socket_wrapper(sock_fd);
    }
    if (is_attr_init) {
        err2 = pthread_attr_destroy(&attr);
        if (err2) {
            LCM_LOG(LCOMMU_LOG_ERROR,
                    "Failed to destroy ptheard attribute(%u):\n", err2);
        }
    }
    return -err;
}

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
comm_lib_tcp_peer_stop(handle_t handle)
{
    int err = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    /* close the socket resource */
    err = close_socket_wrapper(handle);
    lib_commu_bail_error(err);

    /* delete handle DB resource */
    err = lib_commu_db_tcp_handle_info_delete(handle);
    lib_commu_bail_error(err);


bail:
    return -err;
}



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
                           uint32_t *payload_len)
{
    int err = 0;

    struct msg_metadata metadata_st;
    struct handle_info *handle_info_st = NULL;
    enum db_type handle_db_type = ANY_HANLDE_DB;
    uint32_t metadata_len = sizeof(metadata_st);

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(payload);
    lib_commu_bail_null(payload_len);

    if (*payload_len > (MAX_JUMBO_TCP_PAYLOAD - sizeof(metadata_st))) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid payload size [%u]\n", *payload_len);
        lib_commu_bail_force(EOVERFLOW);
    }

    memset((char*) &metadata_st, 0, sizeof(metadata_st));

    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st,
                                       &handle_db_type,
                                       NULL);
    lib_commu_bail_error(err);

    err =
        metadata_set(&metadata_st, MSG_VERSION, *payload_len, *handle_info_st);
    lib_commu_bail_error(err);


    /* sending the metadata */
    err = comm_lib_tcp_ll_send_blocking(handle, (uint8_t*) &metadata_st,
                                        &metadata_len, handle_db_type);
    lib_commu_bail_error(err);

    /* sending the payload */
    err = comm_lib_tcp_ll_send_blocking(handle, payload, payload_len,
                                        handle_db_type);
    lib_commu_bail_error(err);

bail:
    return -err;
}


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
 * @return EBADE - if msg type received on socket is invalid or peer magic is invalid
 * @return EOVERFLOW - if message size > MAX_TCP_PAYLOAD or meesage size = 0
 * @return EPERM - if max_msgs_to_recv > 1, currently the API supports one message each time
 * @return EPERM if library didn't finish init
 */
int
comm_lib_tcp_recv_blocking(handle_t handle, struct addr_info *addresser_st,
                           struct recv_payload_data *payload_data,
                           uint32_t max_msgs_to_recv)
{
    int err = 0;

    struct msg_metadata metadata_st;
    uint32_t metadata_len = sizeof(metadata_st);
    uint32_t payload_len = 0;
    struct handle_info *handle_info_st = NULL;
    enum db_type handle_db_type = ANY_HANLDE_DB;

    memset(&metadata_st, 0, sizeof(metadata_st));

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    /* 1. input validations start*/
    lib_commu_bail_null(addresser_st);
    lib_commu_bail_null(payload_data);

    payload_data->msg_num_recv = 0;
    memset(payload_data->payload_len, 0, sizeof(payload_data->payload_len));
    payload_data->jumbo_payload_len = 0;

    if (max_msgs_to_recv == 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid parameter [max_msgs_to_recv]\n");
        lib_commu_bail_force(EINVAL);
    }

    if (max_msgs_to_recv > 1) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Unsupported parameter [max_msgs_to_recv > 1]\n");
        lib_commu_bail_force(EPERM);
    }

    err = lib_commu_db_hanlde_info_get(handle, &handle_info_st,
                                       &handle_db_type,
                                       NULL);
    lib_commu_bail_error(err);

    if ((handle_db_type != TCP_CLIENT_HANDLE_DB)
        && (handle_db_type != TCP_SERVER_HANDLE_DB)) {
        LCM_LOG(LCOMMU_LOG_ERROR,
                "Invalid handle type[%d]\n", handle_db_type);
        lib_commu_bail_force(EINVAL);
    }
    /*input validations end*/


    addresser_st->ipv4_addr = handle_info_st->conn_info.d_ipv4_addr;
    addresser_st->port = handle_info_st->conn_info.d_port;

    /* 2. recv the metadata and validate*/
    err = comm_lib_tcp_ll_recv_blocking(handle, (uint8_t*) &metadata_st,
                                        &metadata_len);
    lib_commu_bail_error(err);

    if (metadata_len < sizeof(metadata_st)) {
        LCM_LOG(LCOMMU_LOG_ERROR, "metadata incomplete\n");
        lib_commu_bail_force(EIO);
    }

    err = validate_metadata_info(&metadata_st, MAX_JUMBO_TCP_PAYLOAD,
                                 handle_info_st);
    lib_commu_bail_error(err);
    /*recv the metadata and validate end*/

    /* 3. get the message */
    payload_len = metadata_st.payload_size;
    if (metadata_st.payload_size <= MAX_TCP_PAYLOAD) {
        err = comm_lib_tcp_ll_recv_blocking(handle, payload_data->payload[0],
                                            &payload_len);
        lib_commu_bail_error(err);

        payload_data->payload_len[0] = payload_len;
        payload_data->msg_type[0] = metadata_st.msg_type;
    }
    else {
        err = comm_lib_tcp_ll_recv_blocking(handle,
                                            payload_data->jumbo_payload,
                                            &payload_len);
        lib_commu_bail_error(err);

        payload_data->jumbo_payload_len = payload_len;
        payload_data->jumbo_msg_type = metadata_st.msg_type;
    }

    payload_data->msg_num_recv++;


bail:
    return -err;
}


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
 * @return EPERM if library didn't finish init
 */
int
comm_lib_tcp_server_status_get(
    const struct server_status const **server_status,
    uint16_t server_id)
{
    int err = 0;

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(server_status);

    err = lib_commu_db_tcp_server_status_get(server_status, server_id);
    lib_commu_bail_error(err);


bail:
    return -err;
}



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
comm_lib_tcp_handle_status_get(struct connection_status *connection_status)
{
    int err = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    struct sockaddr_in local_addr;
    socklen_t local_addr_len = sizeof(local_addr);

    memset(&peer_addr, 0, sizeof(peer_addr));
    memset(&local_addr, 0, sizeof(local_addr));

    if (!g_lib_commu_init_done) {
        LCM_LOG(LCOMMU_LOG_ERROR, "%s\n", INIT_ERR_MSG);
        lib_commu_bail_force(EPERM);
    }

    lib_commu_bail_null(connection_status);
    if (connection_status->handle == INVALID_HANDLE_ID) {
        LCM_LOG(LCOMMU_LOG_ERROR, "Invalid handle id\n");
        lib_commu_bail_force(EINVAL);
    }

    /* TODO - can add another verification and look for the handle in library DB */

    err = getpeername(connection_status->handle,
                      (struct sockaddr *) &peer_addr,
                      &peer_addr_len);
    if (err != 0) {
        if (errno == ENOTCONN) {
            LCM_LOG(LCOMMU_LOG_INFO, "handle[%d] has no connection\n",
                    connection_status->handle);
            connection_status->handle_status = HANDLE_STATUS_DOWN;
        }
        else {
            LCM_LOG(LCOMMU_LOG_ERROR, "getpeername() failed with err(%d) %s:",
                    errno, strerror(errno));
            lib_commu_bail_force(errno);
        }
    }

    connection_status->handle_status = HANDLE_STATUS_UP;
    connection_status->peer_ipv4_addr = peer_addr.sin_addr.s_addr;
    connection_status->peer_port = peer_addr.sin_port;

    if (getsockname(connection_status->handle, (struct sockaddr*) &local_addr,
                    &local_addr_len) < 0) {
        LCM_LOG(LCOMMU_LOG_ERROR, "getsockname() failed with err(%d): %s",
                errno, strerror(errno));
        lib_commu_bail_force(errno);
    }

    connection_status->local_ipv4_addr = local_addr.sin_addr.s_addr;
    connection_status->local_port = local_addr.sin_port;

bail:
    return err;
}
