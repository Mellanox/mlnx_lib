/*
* Copyright (C) Mellanox Technologies, Ltd. 2001-2013.  ALL RIGHTS RESERVED.
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

#ifndef LIB_EVENT_DISP_H_
#define LIB_EVENT_DISP_H_

#include <stdio.h>
#include <stdbool.h>

/************************************************
 *  Defines
 ***********************************************/

/* 
 * Event dispatcher message data maximum buffer length
 * when not using buffer copy in event generate
 */
#define EVENT_DISP_MAX_BUFF_LEN     (1500)

/* 
 * Event dispatcher maximum client threads
 */
#define EVENT_DISP_MAX_CON          (10)

/* 
 * Event dispatcher maximum supported events
 * Clients MUST define events in the range 0..99
 */
#define EVENT_DISP_MAX_EVENTS       (100)

/* 
 * Event dispatcher APIs returned message length
 */
#define EVENT_DISP_RET_MSG_LEN      (1500)

/************************************************
 *  Macros
 ***********************************************/

/************************************************
 *  Type definitions
 ***********************************************/
/* 
 * Event dispatcher file descriptors structure
 */
typedef struct event_disp_fds {
    int high_fd;
    int med_fd;
    int low_fd;
} event_disp_fds_t;

/* 
 * Event dispatcher message structure
 */
typedef struct event_disp_msg {
    int event;
    char buff[EVENT_DISP_MAX_BUFF_LEN];
} event_disp_msg_t;

/*
 * Event dispatcher total message length
 */
#define EVENT_DISP_MAX_MSG_LEN  (sizeof(event_disp_msg_t))

/*
 * Event dispatcher return code
 */
typedef enum event_disp_status {
    EVENT_DISP_STATUS_SUCCESS = 0,
    EVENT_DISP_STATUS_ALREADY_INIT,
    EVENT_DISP_STATUS_NOT_INIT,
    EVENT_DISP_STATUS_MUTEX_ERROR,
    EVENT_DISP_STATUS_PARAM_NULL,
    EVENT_DISP_STATUS_PARAM_RANGE,
    EVENT_DISP_STATUS_PARAM_INVALID,
    EVENT_DISP_STATUS_MAX_CONNETIONS,
    EVENT_DISP_STATUS_SOCKET_ERROR,
    EVENT_DISP_STATUS_SEND_ERROR,
    EVENT_DISP_STATUS_ERROR,
} event_disp_status_t;

static const char *event_disp_status_t_str[] = {
        "Event dispatcher success",
        "Event dispatcher was already initialized",
        "Event dispatcher is not initialized",
        "Event dispatcher MUTEX error",
        "Event dispatcher pointer is NULL",
        "Event dispatcher parameter is out of range",
        "Event dispatcher parameter is invalid",
        "Event dispatcher exceeds maximum connections",
        "Event dispatcher socket error",
        "Event dispatcher send error",
        "Event dispatcher general error",
};

static const unsigned int event_disp_status_t_str_len = sizeof(event_disp_status_t_str) / sizeof(char *);

#define EVENT_DISPATCHER_STATUS_TO_STR(_idx_)  ((event_disp_status_t_str_len > (_idx_)) ? \
        event_disp_status_t_str[(_idx_)] : "Unknown")

/* 
 * Event dispatcher priorities definition
 */
typedef enum event_disp_priority {
    HIGH_PRIO = 1,
    MED_PRIO = 2,
    LOW_PRIO = 3,
} event_disp_priority_t;

/************************************************
 *  Global variables
 ***********************************************/

/************************************************
 *  Function declarations
 ***********************************************/

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
event_disp_api_init(void);

/**
 *  This function de-initialize event dispatcher library.
 *  It closes all open sockets, removes file descriptors,
 *  resets event dispatcher DB and destroys MUTEX.
 *  If de-init fails, an error indication will be returned,
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
event_disp_api_deinit(void);

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
event_disp_api_open(event_disp_fds_t *fds);

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
event_disp_api_close(event_disp_fds_t *fds);

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
                               unsigned int num_of_events);

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
                                 unsigned int num_of_events);

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
                              unsigned int data_size);

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
                                       unsigned int data_size);

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
                                      unsigned int data_size);

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
 */
event_disp_status_t
event_disp_api_get_event(int fd,
		                 event_disp_msg_t *rcv_msg);

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
 */
event_disp_status_t 
event_disp_api_get_event_to_user_buf(int fd,
                                     char *rcv_msg,
                                     int rcv_size);

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
event_disp_api_dump_data(FILE *dump_file);

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
event_disp_api_get_connections_number(unsigned int *con);

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
event_disp_api_get_event_generation_number(int event, unsigned long int *gen_num);

/**
 *  This function returns event dispatcher connections
 *  number from its DB.
 *
 * @param[in,out] void.
 *
 * @return bool - Event dispatcher init indication flag
 */
bool
event_disp_api_is_init(void);

#endif /* LIB_EVENT_DISP_H_ */
