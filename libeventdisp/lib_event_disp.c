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

#include "lib_event_disp.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <malloc.h>
#include <sys/un.h>
#include <sys/socket.h>

/************************************************
 *  Local Defines
 ***********************************************/
/*
 * UNIX socket file descriptor path name length.
 * Taken from the "sockaddr_un.sun_path" structure.
 */
#define EVENT_DISP_SOCK_PATH_LEN            (108)

/*
 * UNIX socket file descriptor path name prefix.
 * The full path name is "/tmp/event_disp_fd_<fd>"
 */
#define EVENT_DISP_SOCK_PATH_PREFIX         "/tmp/event_disp_fd_"

/*
 * Event dispatcher maximum opened socket.
 * For each client, three sockets are opened.
 */
#define EVENT_DISP_MAX_SOCK                 (EVENT_DISP_MAX_CON * 3)

/*
 * UNIX socket file descriptor path name length.
 * Taken from the "sockaddr_un.sun_path" structure.
 */
#define EVENT_SEND_BLOCKING_MODE            (1)
#define EVENT_SEND_NON_BLOCKING_MODE        (2)
#define EVENT_SEND_NO_COPY                  (1)
#define EVENT_SEND_WITH_COPY                (2)

/************************************************
 *  Local Macros
 ***********************************************/
/************************************************
 *  Local Type definitions
 ***********************************************/
/************************************************
 *  Global variables
 ***********************************************/
/************************************************
 *  Local variables
 ***********************************************/
/*
 * Event dispatcher database:
 * For each event its registered file descriptors.
 */
static int event_disp_db[EVENT_DISP_MAX_EVENTS][EVENT_DISP_MAX_CON];

/*
 * Event dispatcher open sockets FD database
 */
static int event_disp_fds[EVENT_DISP_MAX_SOCK];

/*
 * Event dispatcher connected clients counter
 */
static unsigned int event_disp_con = 0;

/*
 * Events generation counters
 */
static unsigned long int event_disp_gen_counter[EVENT_DISP_MAX_EVENTS];
static unsigned long int event_disp_rcv_counter[EVENT_DISP_MAX_EVENTS];
static unsigned long int event_disp_total_gen_counter = 0;
static unsigned long int event_disp_total_rcv_counter = 0;

/*
 * Event dispatcher MUTEX
 */
static pthread_mutex_t event_disp_mutex;

/*
 * Event dispatcher init flag
 */
static bool event_disp_init = false;

/************************************************
 *  Local function declarations
 ***********************************************/

static event_disp_status_t event_disp_open_socket(int *fd);
static event_disp_status_t event_disp_close_socket(int fd);
static event_disp_status_t event_disp_api_generate_event_mode(int event,
		void *data_buff, unsigned int data_size, int mode, int no_copy);

/************************************************
 *  Function implementations
 ***********************************************/

/*
 *  This function returns a file descriptor of UNIX domain socket.
 *  It opens and binds the socket and updates event dispatcher DB.
 *
 * @param[out] fd - File descriptor (if fails, returns -1).
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
static event_disp_status_t
event_disp_open_socket(int *fd)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    struct sockaddr_un local_sun;
    int local_fd = -1, length = 0;
    unsigned int fd_id = 0;

    /* Validate parameters */
    if (NULL == fd) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    /* Reset structure */
    memset(&local_sun, 0, sizeof(local_sun));

    /* Create socket */
    local_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (-1 == local_fd) {
        err = EVENT_DISP_STATUS_SOCKET_ERROR;
        goto bail;
    }
    /* Set socket path */
    local_sun.sun_family = AF_UNIX;
    snprintf(local_sun.sun_path, sizeof(local_sun.sun_path), "%s%d_%d",
             EVENT_DISP_SOCK_PATH_PREFIX, local_fd, getpid());

    /* Unlink file descriptor if exist */
    unlink(local_sun.sun_path);

    /* Assign path to socket */
    length = strlen(local_sun.sun_path) + sizeof(local_sun.sun_family);
    if (-1 == bind(local_fd, (struct sockaddr *)&local_sun, length)) {
        err = EVENT_DISP_STATUS_SOCKET_ERROR;
        goto bail;
    }
    /* Update file descriptors DB */
    for (fd_id = 0; fd_id < EVENT_DISP_MAX_SOCK; fd_id++) {
        if (-1 == event_disp_fds[fd_id]) {
            event_disp_fds[fd_id] = local_fd;
            break;
        }
    }
    /* Set returned file descriptor */
    *fd = local_fd;

bail:
    if (err) {
        /* If open failed close socket and reset file descriptor */
        if (-1 != local_fd) {
            close(local_fd);
        }
        if (NULL != fd) {
            *fd = -1;
        }
    }
    return err;
}

/*
 *  This function closes a socket, removes the file descriptor
 *  and updates event dispatcher DB.
 *  If socket close fails, an error indication will be returned,
 *  but procedure will not stop.
 *
 * @param[in] fd - File descriptor
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if file descriptor is invalid.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
static event_disp_status_t
event_disp_close_socket(int fd)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    char sun_path[EVENT_DISP_SOCK_PATH_LEN] = {'\0'};
    unsigned int fd_id = 0;

    /* Validate file descriptor */
    if (fd < 0) {
        err = EVENT_DISP_STATUS_PARAM_INVALID;
        goto bail;
    }
    /* Close socket */
    if (-1 == close(fd)) {
        err = EVENT_DISP_STATUS_SOCKET_ERROR;
        /* No bail - continue with unlink and DB update */
    }
    /* Set file descriptor path */
    snprintf(sun_path, sizeof(sun_path), "%s%d_%d", EVENT_DISP_SOCK_PATH_PREFIX,
             fd, getpid());
    /* Unlink file descriptor */
    unlink(sun_path);

    /* Remove file descriptors from DB */
    for (fd_id = 0; fd_id < EVENT_DISP_MAX_SOCK; fd_id++) {
        if (fd == event_disp_fds[fd_id]) {
            event_disp_fds[fd_id] = -1;
            break;
        }
    }

bail:
    return err;
}

/**
 *  This function initialize event dispatcher library.
 *  It resets event dispatcher DB and MUTEX.
 *
 * @param[in,out] void.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_ALREADY_INIT if event dispatcher library was already initialized.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX init fails.
 */
event_disp_status_t
event_disp_api_init(void)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;

    /* Make sure event dispatcher is not already initialized */
    if (true == event_disp_init) {
        err = EVENT_DISP_STATUS_ALREADY_INIT;
        goto bail;
    }
    /* Reset DB */
    memset(event_disp_db, -1, sizeof(event_disp_db));
    memset(event_disp_gen_counter, 0, sizeof(event_disp_gen_counter));
    memset(event_disp_rcv_counter, 0, sizeof(event_disp_rcv_counter));
    event_disp_total_gen_counter = 0;
    event_disp_total_rcv_counter = 0;
    memset(event_disp_fds, -1, sizeof(event_disp_fds));
    event_disp_con = 0;

    /* Initialize MUTEX */
    if (0 != pthread_mutex_init(&event_disp_mutex, NULL)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }
    /* Set event dispatcher library init flag */
    event_disp_init = true;

bail:
    return err;
}

/**
 *  This function de-initialize event dispatcher library.
 *  It closes all open sockets, removes file descriptors,
 *  resets event dispatcher DB and destroys MUTEX.
 *  If deinit fails, an error indication will be returned,
 *  but procedure will continue.
 *
 * @param[in,out] void.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if file descriptor is invalid.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 */
event_disp_status_t
event_disp_api_deinit(void)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    event_disp_status_t tmp_err = EVENT_DISP_STATUS_SUCCESS;
    unsigned int fd_id = 0;

    /* Make sure event dispatcher was initialized */
    if (false == event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Close all opened sockets */
    for (fd_id = 0; fd_id < EVENT_DISP_MAX_SOCK; fd_id++) {
        if (event_disp_fds[fd_id] >= 0) {
            /* Close all sockets */
            tmp_err = event_disp_close_socket(event_disp_fds[fd_id]);
            /* Continue even if fail, since deinit is best effort */
            if ((tmp_err) && (!err)) {
                /* Update status */
                err = tmp_err;
            }
        }
    }
    /* Reset DB */
    memset(event_disp_db, -1, sizeof(event_disp_db));
    memset(event_disp_gen_counter, 0, sizeof(event_disp_gen_counter));
    memset(event_disp_rcv_counter, 0, sizeof(event_disp_rcv_counter));
    event_disp_total_gen_counter = 0;
    event_disp_total_rcv_counter = 0;
    memset(event_disp_fds, -1, sizeof(event_disp_fds));
    event_disp_con = 0;

    /* Destroy MUTEX */
    if (0 != pthread_mutex_destroy(&event_disp_mutex)) {
        /* Continue even if fail, since deinit is best effort */
        if (!err) {
            /* Update status */
            err = EVENT_DISP_STATUS_MUTEX_ERROR;
        }
    }
    /* Set event dispatcher library init flag */
    event_disp_init = false;

bail:
    return err;
}

/**
 *  This function returns three file descriptors of UNIX domain sockets.
 *  It opens and binds sockets and updates event dispatcher DB.
 *  The returned file descriptors can be used to receive events,
 *  and implement different priority for each one.
 *
 * @param[out] fds - Returned file descriptors structure (if fails, resets all FD to -1).
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 * @return EVENT_DISP_STATUS_MAX_CONNETIONS if no more connections are available.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
event_disp_status_t
event_disp_api_open(event_disp_fds_t *fds)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    bool mutex_lock = false;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate input */
    if (NULL == fds) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    /* Lock MUTEX */
    if (0 != pthread_mutex_lock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }
    mutex_lock = true;

    /* Check number of connections */
    if (event_disp_con >= EVENT_DISP_MAX_CON) {
        err = EVENT_DISP_STATUS_MAX_CONNETIONS;
        goto bail;
    }
    /* Reset file descriptors */
    memset(fds, -1, sizeof(*fds));

    /* Create high priority socket */
    err = event_disp_open_socket(&fds->high_fd);
    if (err) {
        goto bail;
    }
    /* Create medium priority socket */
    err = event_disp_open_socket(&fds->med_fd);
    if (err) {
        goto bail;
    }
    /* Create low priority socket */
    err = event_disp_open_socket(&fds->low_fd);
    if (err) {
        goto bail;
    }
    /* Update connection number */
    event_disp_con++;

bail:
    if ((err) && (NULL != fds)) {
        /* If open failed need to close all sockets */
        if (-1 != fds->high_fd) {
            event_disp_close_socket(fds->high_fd);
        }
        if (-1 != fds->med_fd) {
            event_disp_close_socket(fds->med_fd);
        }
        if (-1 != fds->low_fd) {
            event_disp_close_socket(fds->low_fd);
        }
    }
    /* Unlock MUTEX */
    if (true == mutex_lock) {
        pthread_mutex_unlock(&event_disp_mutex);
    }
    return err;
}

/**
 *  This function closes the sockets that where opened using
 *  event_disp_api_open, and removes the file descriptors.
 *  The returned file descriptors are set to -1.
 *  If close fails, an error indication will be returned,
 *  but procedure will continue.
 *
 * @param[in] fds - File descriptors structure.
 * @param[out] fds - Returned file descriptors structure reseted to -1.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if file descriptor is invalid.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
event_disp_status_t
event_disp_api_close(event_disp_fds_t *fds)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    event_disp_status_t tmp_err = EVENT_DISP_STATUS_SUCCESS;
    unsigned int event_id = 0, con_id = 0;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate input */
    if (NULL == fds) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    /* Lock MUTEX */
    if (0 != pthread_mutex_lock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }
    /* Remove FDs from event registrations DB */
    for (event_id = 0; event_id < EVENT_DISP_MAX_EVENTS; event_id++) {
        for (con_id = 0; con_id < EVENT_DISP_MAX_CON; con_id++) {
            int temp_fd = event_disp_db[event_id][con_id];
            if ((fds->high_fd == temp_fd) ||
                (fds->med_fd == temp_fd) ||
                (fds->low_fd == temp_fd)) {
                /* Reset file descriptor data */
                event_disp_db[event_id][con_id] = -1;
            }
        }
    }
    /* Close connections and unlink file descriptors */
    tmp_err = event_disp_close_socket(fds->high_fd);
    if (!err) {
        err = tmp_err;
    }
    tmp_err = event_disp_close_socket(fds->med_fd);
    if (!err) {
        err = tmp_err;
    }
    tmp_err = event_disp_close_socket(fds->low_fd);
    if (!err) {
        err = tmp_err;
    }
    /* Unlock MUTEX */
    if (0 != pthread_mutex_unlock(&event_disp_mutex)) {
        if (!err) {
            err = EVENT_DISP_STATUS_MUTEX_ERROR;
        }
    }

bail:
    return err;
}

/**
 *  This function registers events to a specific file descriptor
 *  based on its priority, and updates event dispatcher DB.
 *  It first unregister the events in order to prevent duplicate
 *  priority registration.
 *
 * @param[in] fds - File descriptors structure.
 * @param[in] prio - Event priority (HIGH_PRIO = 1,MED_PRIO = 2,LOW_PRIO = 3).
 * @param[in] events - Events list. Event number must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[in] num_of_events - Number of events in list, must be between
 *            1 and EVENT_DISP_MAX_EVENTS.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameters pointers are NULL.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if number of events exceeds range.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if priority is invalid.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 */
event_disp_status_t
event_disp_api_register_events(const event_disp_fds_t *fds,
                               event_disp_priority_t prio,
                               const int *events,
                               unsigned int num_of_events)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    unsigned int event_list_id = 0, con_id = 0;
    int fd = -1;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate input */
    if (NULL == fds) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if (NULL == events) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if ((num_of_events == 0) ||
        (num_of_events > EVENT_DISP_MAX_EVENTS)) {
        err = EVENT_DISP_STATUS_PARAM_RANGE;
        goto bail;
    }
    /* We first unregister client from the events, to make sure that if client was already
     * registered to some of these events with different priority, it will be replaced */
    err = event_disp_api_unregister_events(fds, events, num_of_events);
    if (err) {
        goto bail;
    }
    /* Set file descriptor according to priority */
    switch (prio) {
    case HIGH_PRIO:
        fd = fds->high_fd;
        break;
    case MED_PRIO:
        fd = fds->med_fd;
        break;
    case LOW_PRIO:
        fd = fds->low_fd;
        break;
    default:
        err = EVENT_DISP_STATUS_PARAM_INVALID;
        goto bail;
        /* no break */
    }
    /* Validate file descriptor */
    if (fd < 0) {
        err = EVENT_DISP_STATUS_PARAM_INVALID;
        goto bail;
    }
    /* Lock MUTEX */
    if (0 != pthread_mutex_lock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }
    /* Set file descriptor to events database */
    for (event_list_id = 0; event_list_id < num_of_events; event_list_id++) {
        /* Validate event is in range */
        if ((events[event_list_id] < EVENT_DISP_MAX_EVENTS) &&
            (events[event_list_id] >= 0)) {
            /* Find free entry in database */
            for (con_id = 0; con_id < EVENT_DISP_MAX_CON; con_id++) {
                if (-1 == event_disp_db[events[event_list_id]][con_id]) {
                    /* Set FD to open slot */
                    event_disp_db[events[event_list_id]][con_id] = fd;
                    /* Continue to next event in the list */
                    break;
                }
            }
        }
    }
    /* Unlock MUTEX */
    if (0 != pthread_mutex_unlock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }

bail:
    return err;
}

/**
 *  This function unregisters client from events,
 *  and updates event dispatcher DB.
 *
 * @param[in] fds - File descriptors structure.
 * @param[in] events - Events list. Event number must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[in] num_of_events - Number of events in list, must be between
 *            1 and EVENT_DISP_MAX_EVENTS.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameters pointers are NULL.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if number of events exceeds range.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 */
event_disp_status_t
event_disp_api_unregister_events(const event_disp_fds_t *fds,
                                 const int *events,
                                 unsigned int num_of_events)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    unsigned int event_list_id = 0, con_id = 0;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate input */
    if (NULL == fds) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if (NULL == events) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if ((num_of_events == 0) ||
        (num_of_events > EVENT_DISP_MAX_EVENTS)) {
        err = EVENT_DISP_STATUS_PARAM_RANGE;
        goto bail;
    }
    /* Lock MUTEX */
    if (0 != pthread_mutex_lock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }
    /* Remove file descriptors from registered events database */
    for (event_list_id = 0; event_list_id < num_of_events; event_list_id++) {
        /* Validate event is in range */
        if ((events[event_list_id] < EVENT_DISP_MAX_EVENTS) &&
            (events[event_list_id] >= 0)) {
            /* Find the entry in database */
            for (con_id = 0; con_id < EVENT_DISP_MAX_CON; con_id++) {
                if ((fds->high_fd ==
                     event_disp_db[events[event_list_id]][con_id]) ||
                    (fds->med_fd ==
                     event_disp_db[events[event_list_id]][con_id]) ||
                    (fds->low_fd ==
                     event_disp_db[events[event_list_id]][con_id])) {
                    /* Reset FD to DB */
                    event_disp_db[events[event_list_id]][con_id] = -1;
                }
            }
        }
    }
    /* Unlock MUTEX */
    if (0 != pthread_mutex_unlock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }

bail:
    return err;
}

/**
 *  This function generates event, sends it to all
 *  registered clients in non-blocking mode without copy of message body to
 *  the temporary buffer.
 *
 * @param[in] event - Event type, must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[in] data_buff - Event data (accepts NULL).
 * @param[in] data_size - Data buffer size (accepts 0).
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if parameters exceeds range.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
event_disp_status_t
event_disp_api_generate_event_no_copy(int event,
                                      void *data_buff,
                                      unsigned int data_size)
{
	return event_disp_api_generate_event_mode(event, data_buff, data_size,
											  EVENT_SEND_NON_BLOCKING_MODE,
											  EVENT_SEND_NO_COPY);
}

/**
 *  This function generates event, sends it to all
 *  registered clients in non-blocking mode,
 *  and updates event dispatcher DB.
 *
 * @param[in] event - Event type, must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[in] data_buff - Event data (accepts NULL).
 * @param[in] data_size - Data buffer size (accepts 0).
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if parameters exceeds range.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
event_disp_status_t
event_disp_api_generate_event(int event,
                              void *data_buff,
                              unsigned int data_size)
{
	return event_disp_api_generate_event_mode(event, data_buff, data_size,
											  EVENT_SEND_NON_BLOCKING_MODE,
											  EVENT_SEND_WITH_COPY);
}

/**
 *  This function generates event, sends it to all
 *  registered clients in blocking mode,
 *  and updates event dispatcher DB.
 *
 * @param[in] event - Event type, must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[in] data_buff - Event data (accepts NULL).
 * @param[in] data_size - Data buffer size (accepts 0).
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if parameters exceeds range.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
event_disp_status_t
event_disp_api_generate_event_blocking(int event,
                                       void *data_buff,
                                       unsigned int data_size)
{
	return event_disp_api_generate_event_mode(event, data_buff, data_size,
											  EVENT_SEND_BLOCKING_MODE,
											  EVENT_SEND_WITH_COPY);
}

/*
 *  This function generates event, sends it to all
 *  registered clients, and updates event dispatcher DB.
 *
 * @param[in] event - Event type, must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[in] data_buff - Event data (accepts NULL).
 * @param[in] data_size - Data buffer size (accepts 0)
 * @param[in] mode - Mode to send (EVENT_SEND_BLOCKING_MODE
 *                   or EVENT_SEND_NON_BLOCKING_MODE).
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if parameters exceeds range.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 */
static event_disp_status_t
event_disp_api_generate_event_mode(int event,
								   void *data_buff,
                                   unsigned int data_size,
                                   int mode,
                                   int no_copy)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    event_disp_status_t send_err = EVENT_DISP_STATUS_SUCCESS;
    struct sockaddr_un local_sun;
    int socket_fd = -1;
    int length = 0;
    event_disp_msg_t msg;
    size_t send_bytes = 0;
    unsigned int con_id = 0;
    bool mutex_lock = false;
    unsigned short opcode;
    void *buf = NULL;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate inputs */
    if ((no_copy == EVENT_SEND_WITH_COPY) &&
          (data_size > EVENT_DISP_MAX_BUFF_LEN)) {
        err = EVENT_DISP_STATUS_PARAM_RANGE;
        goto bail;
    }
    if ((event >= EVENT_DISP_MAX_EVENTS) ||
        (event < 0)) {
        err = EVENT_DISP_STATUS_PARAM_RANGE;
        goto bail;
    }
    /* Reset structures */
    memset(&local_sun, 0, sizeof(local_sun));
    memset(&msg, 0, sizeof(msg));

    /* Lock MUTEX */
    if (0 != pthread_mutex_lock(&event_disp_mutex)) {
        err = EVENT_DISP_STATUS_MUTEX_ERROR;
        goto bail;
    }
    mutex_lock = true;

    /* Prepare message */
    if (no_copy == EVENT_SEND_NO_COPY) {
    	if ((NULL != data_buff) && (0 != data_size)) {
    		*(unsigned short*)data_buff = (unsigned short)event;
    	}
    }
    else if (no_copy == EVENT_SEND_WITH_COPY) {
    	msg.event = event;
    	if ((NULL != data_buff) && (0 != data_size)) {
    		memcpy(msg.buff, data_buff, data_size);
    	}
	}

    /* Go over event registration database */
    for (con_id = 0; con_id < EVENT_DISP_MAX_CON; con_id++) {
        /* Close connection from last iteration */
        if ((-1 != socket_fd) && (-1 == close(socket_fd))) {
            err = EVENT_DISP_STATUS_SOCKET_ERROR;
            goto bail;
        }
        /* Reset connection parameters */
        socket_fd = -1;
        length = 0;
        memset(&local_sun, 0, sizeof(local_sun));

        /* Find a registered FD */
        if (-1 != event_disp_db[event][con_id]) {
            /* Create UNIX domain socket */
            socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
            if (-1 == socket_fd) {
                err = EVENT_DISP_STATUS_SOCKET_ERROR;
                goto bail;
            }
            /* Check on requested mode to send */
            if (mode == EVENT_SEND_NON_BLOCKING_MODE) {
            	/* Set non-blocking flag on socket */
            	if (-1 == fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
            		err = EVENT_DISP_STATUS_SOCKET_ERROR;
            		goto bail;
            	}
            }
            /* Set socket path */
            local_sun.sun_family = AF_UNIX;
            snprintf(local_sun.sun_path, sizeof(local_sun.sun_path), "%s%d_%d",
                     EVENT_DISP_SOCK_PATH_PREFIX,
                     event_disp_db[event][con_id], getpid());
            length = strlen(local_sun.sun_path) + sizeof(local_sun.sun_family);

            /* Send event */
            if (no_copy == EVENT_SEND_WITH_COPY) {
               	send_bytes = (char *)msg.buff - (char *)&msg + data_size;
               	buf = (void *)&msg;
            }
            else if (no_copy == EVENT_SEND_NO_COPY) {
				if ((NULL != data_buff) && (0 != data_size)) {
					send_bytes = data_size;
					buf = data_buff;
				}
				else {
					opcode = (unsigned short)event;
					send_bytes = sizeof(opcode);
					buf = &opcode;
				}
            }
        	if (-1 == sendto(socket_fd, buf, send_bytes, 0,
                             (struct sockaddr *)&local_sun,
                             (socklen_t)length)) {
        		/* If send fails we continue to next registered FD */
        		if ((EAGAIN == errno) || (EWOULDBLOCK == errno)){
        			/* Send sometimes fail due to thread deinit before closing
        			 * sockets, no need to return error */
        			continue;
        		}else{
        			/* Real error */
        			send_err = EVENT_DISP_STATUS_SEND_ERROR;
        		}
        	}
        }
    }
    /* Increase generation counter */
    event_disp_gen_counter[event]++;
    event_disp_total_gen_counter++;

bail:
    /* If no error, set return send status */
    if (!err){
        err = send_err;
    }
    /* Close socket */
    if (-1 != socket_fd) {
        close(socket_fd);
    }
    if (true == mutex_lock) {
        /* Unlock MUTEX */
        pthread_mutex_unlock(&event_disp_mutex);
    }
    return err;
}

/**
 *  This function returns event message data,
 *  that was received on an event dispatcher socket.
 *
 * @param[in] fd - File descriptor.
 * @param[out] rcv_msg - Returned event message.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if file descriptor is invalid.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if received event outside valid range.
 */
event_disp_status_t
event_disp_api_get_event(int fd,
		                 event_disp_msg_t *rcv_msg)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;

    /* Validate input */
    if (NULL == rcv_msg) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if (fd < 0) {
        err = EVENT_DISP_STATUS_PARAM_INVALID;
        goto bail;
    }

    /* Receive data */
    if (-1 == recv(fd, rcv_msg, sizeof(*rcv_msg), 0)) {
        err = EVENT_DISP_STATUS_SOCKET_ERROR;
        goto bail;
    }

    /* Verify received event within range */
    if ((rcv_msg->event >= EVENT_DISP_MAX_EVENTS) || (rcv_msg->event < 0)) {
        err = EVENT_DISP_STATUS_PARAM_RANGE;
        goto bail;
    }

    /* Increase receive counter */
    event_disp_rcv_counter[rcv_msg->event]++;
    event_disp_total_rcv_counter++;

bail:
    return err;
}

/**
 *  This function returns event message data,
 *  that was received on an event dispatcher socket
 *  to buffer given by user.
 *
 * @param[in] fd - File descriptor.
 * @param[in/out] rcv_msg - User buffer to return event message.
 * @param[in] rcv_size - User buffer size.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if file descriptor is invalid.
 * @return EVENT_DISP_STATUS_SOCKET_ERROR if socket operation fails.
 * @return EVENT_DISP_STATUS_PARAM_RANGE if received event outside valid range.
 */
event_disp_status_t
event_disp_api_get_event_to_user_buf(int fd,
		                 char *rcv_msg,
		                 int rcv_size)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    event_disp_msg_t    *ed_msg = (event_disp_msg_t *) rcv_msg;

    /* Validate input */
    if (NULL == rcv_msg) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if (fd < 0) {
        err = EVENT_DISP_STATUS_PARAM_INVALID;
        goto bail;
    }

    /* Receive data */
    if (-1 == recv(fd, rcv_msg, rcv_size, 0)) {
        err = EVENT_DISP_STATUS_SOCKET_ERROR;
        goto bail;
    }

    /* Increase event receive counter for a message with a valid event */
    if ((ed_msg->event >= 0) && (ed_msg->event < EVENT_DISP_MAX_EVENTS)) {
        event_disp_rcv_counter[ed_msg->event]++;
    }

    /* Increase receive counter */
    event_disp_total_rcv_counter++;

bail:
    return err;
}

/**
 *  This function prints event dispatcher database to file.
 *
 * @param[in,out] dump_file - File stream pointer.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_MUTEX_ERROR if MUTEX operation fails.
 */
event_disp_status_t
event_disp_api_dump_data(FILE *dump_file)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;
    unsigned int event_id = 0, con_id = 0;
    bool is_reg = false;

    /* Validate input */
    if (NULL == dump_file) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    /* Header */
    fprintf(dump_file, "\n\n===============================\n");
    fprintf(dump_file, "Printing event dispatcher data:\n");
    fprintf(dump_file, "===============================\n\n");

    /* Check library is initialized */
    if (false == event_disp_init) {
        fprintf(dump_file, "Event dispatcher library is no initialized\n");
    }
    else {
        /* Lock MUTEX */
        if (0 != pthread_mutex_lock(&event_disp_mutex)) {
            err = EVENT_DISP_STATUS_MUTEX_ERROR;
            goto bail;
        }
        /* Dump database  */
        fprintf(dump_file, "Events dispatcher database:\n");
        fprintf(dump_file, "========================\n");
        fprintf(dump_file, "Number of connections = %u\n", event_disp_con);
        for (event_id = 0; event_id < EVENT_DISP_MAX_EVENTS; event_id++) {
            is_reg = false;
            fprintf(dump_file, "\nEvent %d:\n", event_id);
            fprintf(dump_file, "Generated %lu times, received %lu times\n",
                    event_disp_gen_counter[event_id],
                    event_disp_rcv_counter[event_id]);
            fprintf(dump_file, "Registered clients -\n");
            for (con_id = 0; con_id < EVENT_DISP_MAX_CON; con_id++) {
                if (-1 != event_disp_db[event_id][con_id]) {
                    fprintf(dump_file, "FD %d\n",
                            event_disp_db[event_id][con_id]);
                    is_reg = true;
                }
            }
            if (!is_reg) {
                fprintf(dump_file, "No FDs\n");
            }
        }
        fprintf(dump_file,
                "\nTotal %lu generated, %lu received\n",
                event_disp_total_gen_counter,
                event_disp_total_rcv_counter);
        /* Unlock MUTEX */
        if (0 != pthread_mutex_unlock(&event_disp_mutex)) {
            err = EVENT_DISP_STATUS_MUTEX_ERROR;
            goto bail;
        }
    }
    fprintf(dump_file, "\n===========================================\n\n");

bail:
    return err;
}

/**
 *  This function returns event dispatcher connections
 *  number from its DB.
 *
 * @param[out] con - Returned connections number.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 */
event_disp_status_t
event_disp_api_get_connections_number(unsigned int *con)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate input */
    if (NULL == con) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }

    /* Set returned connections number */
    *con = event_disp_con;

bail:
    return err;
}

/**
 *  This function returns event dispatcher connections
 *  number from its DB.
 *
 * @param[in] event - Event type, must be between
 *            0 and (EVENT_DISP_MAX_EVENTS - 1).
 * @param[out] gen_num - Returned generation number.
 *
 * @return EVENT_DISP_STATUS_SUCCESS if operation completes successfully.
 * @return EVENT_DISP_STATUS_NOT_INIT if event dispatcher library is not initialized.
 * @return EVENT_DISP_STATUS_PARAM_NULL if parameter is NULL.
 * @return EVENT_DISP_STATUS_PARAM_INVALID if event is invalid.
 */
event_disp_status_t
event_disp_api_get_event_generation_number(int event,
                                           unsigned long int *gen_num)
{
    event_disp_status_t err = EVENT_DISP_STATUS_SUCCESS;

    /* Check init flag */
    if (true != event_disp_init) {
        err = EVENT_DISP_STATUS_NOT_INIT;
        goto bail;
    }
    /* Validate input */
    if (NULL == gen_num) {
        err = EVENT_DISP_STATUS_PARAM_NULL;
        goto bail;
    }
    if ((event >= EVENT_DISP_MAX_EVENTS) ||
        (event < 0)) {
        err = EVENT_DISP_STATUS_PARAM_RANGE;
        goto bail;
    }
    /* Set returned generations number */
    *gen_num = event_disp_gen_counter[event];

bail:
    return err;

}

/**
 *  This function returns event dispatcher connections
 *  number from its DB.
 *
 * @param[in,out] void.
 *
 * @return bool - Event dispatcher init indication flag
 */
bool
event_disp_api_is_init(void)
{
    return event_disp_init;
}

