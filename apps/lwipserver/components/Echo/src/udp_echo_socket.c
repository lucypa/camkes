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

#include "client.h"
#include <lwip/err.h>

int udp_socket = -1;

int handle_udp_socket_async_sent(tx_msg_t *msg)
{
    virtqueue_ring_object_t handle;
    msg->total_len = UDP_READ_SIZE;
    msg->done_len = 0;
    virtqueue_init_ring_object(&handle);
    if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(msg),
                                     BUF_SIZE, VQ_RW)) {
        ZF_LOGF("handle_udp_socket_async_sent: Error when queueing avail buffer");
    }
    return 0;
}

int handle_udp_socket_async_received(tx_msg_t *msg)
{
    virtqueue_ring_object_t handle;
    virtqueue_init_ring_object(&handle);
    if (msg->done_len == -1 || msg->done_len == 0) {
        /* Error on this request, re-send it */
        msg->total_len = UDP_READ_SIZE;
        msg->done_len = 0;
        if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(msg),
                                         BUF_SIZE, VQ_RW)) {
            ZF_LOGF("handle_udp_socket_async_received: Error when queueing avail buffer to RX virtqueue");
        }
    } else {
        /* Move the buffer to the TX queue to be echoed back */
        msg->total_len = msg->done_len;
        msg->done_len = 0;
        if (!virtqueue_add_available_buf(&tx_virtqueue, &handle, ENCODE_DMA_ADDRESS(msg),
                                         sizeof(*msg), VQ_RW)) {
            ZF_LOGF("handle_udp_socket_async_received: Error when queueing avail buffer to TX virtqueue");
        }
    }

    return 0;
}

static int setup_udp_echo_socket(ps_io_ops_t *io_ops)
{
    udp_socket = echo_control_open(true);
    ZF_LOGF_IF(udp_socket < 0, "Failed to open a UDP socket for benchmarks");

    int ret = echo_control_bind(udp_socket, *IP_ANY_TYPE, UDP_ECHO_PORT);
    ZF_LOGF_IF(ret, "Failed to bind a UDP socket");

    ret = echo_control_set_async(udp_socket, true);
    ZF_LOGF_IF(ret, "Failed to set UDP socket to async");

    for (int i = 0; i < NUM_UDP_BUFS; i++) {
        tx_msg_t *buf = ps_dma_alloc(&io_ops->dma_manager, BUF_SIZE, 4, 1, PS_MEM_NORMAL);
        ZF_LOGF_IF(buf == NULL, "Failed to allocate UDP DMA memory");
        memset(buf, 0, BUF_SIZE);
        buf->socket_fd = udp_socket;
        buf->total_len = UDP_READ_SIZE;
        buf->done_len = 0;
        buf->client_cookie = (void *) UDP_SOCKETS_ASYNC_ID;

        virtqueue_ring_object_t handle;
        virtqueue_init_ring_object(&handle);

        if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(buf),
                                         sizeof(*buf), VQ_RW)) {
            ZF_LOGF("Failed to add the UDP DMA buf to the RX virtqueue");
        }
    }

    return 0;
}

CAMKES_POST_INIT_MODULE_DEFINE(setup_udp, setup_udp_echo_socket);
