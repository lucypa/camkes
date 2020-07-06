/*
 * Copyright 2020, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#pragma once

#include <camkes.h>
#include <autoconf.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <camkes/dataport.h>
#include <camkes/io.h>
#include <camkes/msgqueue.h>
#include <camkes/virtqueue.h>
#include <lwipserver.h>
#include <echo/tuning_params.h>

#include "ports.h"

/* Async virtqueue identifiers */
#define TCP_SOCKETS_ASYNC_ID 1
#define UDP_SOCKETS_ASYNC_ID 2

static_assert(BUF_SIZE >= (sizeof(tx_msg_t) + UDP_READ_SIZE),
              "BUF_SIZE is too small to hold UDP_READ_SIZE of data and metadata");
static_assert(BUF_SIZE >= (sizeof(tx_msg_t) + TCP_READ_SIZE),
              "BUF_SIZE is too small to hold UDP_READ_SIZE of data and metadata");

extern int tcp_socket;
extern int peer_tcp_socket;

extern int udp_socket;

extern int utilization_socket;
extern int peer_utilization_socket;

/* Transports: shared buffers for synchronous, virtqueues for asynchronous */
extern void *echo_recv_buf;
extern void *echo_send_buf;
extern virtqueue_driver_t tx_virtqueue;
extern virtqueue_driver_t rx_virtqueue;

int handle_tcp_event(lwipserver_event_t *);
int handle_tcp_socket_async_sent(tx_msg_t *);
int handle_tcp_socket_async_received(tx_msg_t *);

int handle_udp_socket_async_sent(tx_msg_t *);
int handle_udp_socket_async_received(tx_msg_t *);

int handle_utilization_event(lwipserver_event_t *);
