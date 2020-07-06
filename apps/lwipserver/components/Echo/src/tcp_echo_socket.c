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

int tcp_socket = -1;
int peer_tcp_socket = -1;

static int num_rx_bufs = 0;
static tx_msg_t *rx_buf_pool[NUM_TCP_BUFS];

int handle_tcp_socket_async_received(tx_msg_t *msg)
{
    virtqueue_ring_object_t handle;

    if (msg->socket_fd != peer_tcp_socket) {
        /* Socket has been closed already */
        if (peer_tcp_socket == -1) {
            ZF_LOGE("socket's been closed");
            rx_buf_pool[num_rx_bufs] = msg;
            num_rx_bufs++;
            return 0;
        } else {
            ZF_LOGE("re-using the packets");
            msg->socket_fd = peer_tcp_socket;
            msg->done_len = -1;
        }
    }

    virtqueue_init_ring_object(&handle);
    if (msg->done_len == -1 || msg->done_len == 0) {
        /* Error on the receive request, re-send it */
        msg->total_len = TCP_READ_SIZE;
        msg->done_len = 0;
        if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(msg),
                                         BUF_SIZE, VQ_RW)) {
            ZF_LOGF("tcp_socket_handle_async_recevied: Error while enqueuing available buffer");
        }
    } else {
        /* Move the packet to the TX queue for it be echoed back */
        msg->total_len = msg->done_len;
        msg->done_len = 0;
        if (!virtqueue_add_available_buf(&tx_virtqueue, &handle, ENCODE_DMA_ADDRESS(msg), sizeof(*msg), VQ_RW)) {
            ZF_LOGF("tcp_socket_handle_async_recevied: Error while enqueuing available buffer");
        }
    }
    return 0;
}

int handle_tcp_socket_async_sent(tx_msg_t *msg)
{
    virtqueue_ring_object_t handle;
    msg->total_len = TCP_READ_SIZE;
    msg->done_len = 0;
    if (msg->socket_fd != peer_tcp_socket) {
        /* Socket has been closed already */
        if (peer_tcp_socket == -1) {
            rx_buf_pool[num_rx_bufs] = msg;
            num_rx_bufs++;
            return 0;
        } else {
            msg->socket_fd = peer_tcp_socket;
        }
    }
    virtqueue_init_ring_object(&handle);
    if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(msg),
                                     BUF_SIZE, VQ_RW)) {
        ZF_LOGF("tcp_socket_handle_async_sent: Error while enqueueing available buffer");
    }
    return 0;
}

int handle_tcp_event(lwipserver_event_t *event)
{
    int error = 0;
    ip_addr_t peer_addr;
    char ip_string[IP4ADDR_STRLEN_MAX + 1] = {0};
    uint16_t peer_port;

    if (event->type & LWIPSERVER_PEER_CLOSED) {
        assert(event->socket_fd == peer_tcp_socket);
        error = echo_control_close(peer_tcp_socket);
        ZF_LOGF_IF(error, "Failed to close the peer TCP socket");
        printf("Closed peer socket on %d\n", peer_tcp_socket);
        peer_tcp_socket = -1;
    }

    if (event->type & LWIPSERVER_PEER_AVAIL) {
        assert(event->socket_fd == tcp_socket);
        error = echo_control_accept(tcp_socket, &peer_addr, &peer_port,
                                    &peer_tcp_socket);
        ZF_LOGF_IF(error, "Failed to accept the new peer");

        error = echo_control_set_async(peer_tcp_socket, true);
        ZF_LOGF_IF(error, "Failed to set peer TCP socket to be async");

        /* Add all the DMA bufs to the RX queue */
        int prev_num_rx_bufs = num_rx_bufs;
        while (num_rx_bufs > 0) {
            virtqueue_ring_object_t handle;
            virtqueue_init_ring_object(&handle);
            num_rx_bufs--;
            tx_msg_t *buf = rx_buf_pool[num_rx_bufs];
            buf->total_len = TCP_READ_SIZE;
            buf->done_len = 0;
            buf->socket_fd = peer_tcp_socket;
            if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(buf),
                                             sizeof(*buf), VQ_RW)) {
                ZF_LOGF("Failed to queue all the TCP DMA bufs to the RX queue");
            }
        }
        if (prev_num_rx_bufs != num_rx_bufs) {
            ZF_LOGE("added %d DMA bufs to the queue", prev_num_rx_bufs - num_rx_bufs);
        }
        ip4addr_ntoa_r(&peer_addr, ip_string, IP4ADDR_STRLEN_MAX);
        printf("New peer connected from %s:%hu on socket %d\n", ip_string, peer_port,
               peer_tcp_socket);
    }

    return 0;
}

static int setup_tcp_echo_socket(ps_io_ops_t *io_ops)
{
    tcp_socket = echo_control_open(false);
    ZF_LOGF_IF(tcp_socket < 0, "Failed to open a socket for listening!");

    int ret = echo_control_bind(tcp_socket, *IP_ANY_TYPE, TCP_ECHO_PORT);
    ZF_LOGF_IF(ret, "Failed to bind a socket for listening!");

    ret = echo_control_listen(tcp_socket, 10);
    ZF_LOGF_IF(ret, "Failed to listen for incoming connections!");

    for (int i = 0; i < NUM_TCP_BUFS; i++) {
        tx_msg_t *buf = ps_dma_alloc(&io_ops->dma_manager, BUF_SIZE, 4, 1, PS_MEM_NORMAL);
        ZF_LOGF_IF(buf == NULL, "Failed to alloc DMA memory for the TCP socket");
        memset(buf, 0, BUF_SIZE);
        buf->socket_fd = -1;
        buf->client_cookie = (void *) TCP_SOCKETS_ASYNC_ID;
        rx_buf_pool[num_rx_bufs] = buf;
        num_rx_bufs++;
    }

    return 0;
}

CAMKES_POST_INIT_MODULE_DEFINE(setup_tcp, setup_tcp_echo_socket);
