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

#define NUM_UDP_BUFS 510
#define NUM_TCP_BUFS 510

#define TCP_READ_SIZE 1400
#define UDP_READ_SIZE 1400

#define BUF_SIZE 2048

#define LWIP_SOCKET_ASYNC_QUEUE_LEN 1024
#define LWIP_SOCKET_ASYNC_POOL_SIZE (BUF_SIZE * LWIP_SOCKET_ASYNC_QUEUE_LEN)
#define LWIPSERVER_HEAP_SIZE 0x800000
