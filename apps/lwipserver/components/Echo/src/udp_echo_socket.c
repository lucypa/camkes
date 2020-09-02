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
    //trace_extra_point_start(2 + 1);
    msg->total_len = UDP_READ_SIZE;
    msg->done_len = 0;
    msg->buf_ref = NULL;

    tx_msg_t *rx_request;
    int ret = camkes_virtqueue_buffer_alloc(&rx_virtqueue, (void **) &rx_request, sizeof(*rx_request));
    if (ret) {
        ZF_LOGF("Failed to request a buffer from the rx virtqueue");
    }

    *rx_request = *msg;

    ret = camkes_virtqueue_driver_send_buffer(&rx_virtqueue, rx_request, sizeof(*rx_request));
    ZF_LOGF_IF(ret != 0, "Failed to add request to TX virtqueue");
    camkes_virtqueue_buffer_free(&tx_virtqueue, msg);
    //trace_extra_point_end(2 + 1, 1);

    return 0;
}

int handle_udp_socket_async_received(tx_msg_t *msg)
{
    //trace_extra_point_start(2 + 2);
    virtqueue_ring_object_t handle;
    virtqueue_init_ring_object(&handle);
    if (msg->done_len == -1 || msg->done_len == 0) {
        /* Error on this request, re-send it */
        msg->total_len = UDP_READ_SIZE;
        msg->done_len = 0;
        if (!camkes_virtqueue_driver_send_buffer(&rx_virtqueue, msg, sizeof(*msg))) {
            ZF_LOGF("Error when queueing buffer to RX virtqueue");
        }
    } else {
        /* Move the buffer to the TX queue to be echoed back */
        msg->total_len = msg->done_len;
        msg->done_len = 0;

        tx_msg_t *echo_msg;
        int ret = camkes_virtqueue_buffer_alloc(&tx_virtqueue, (void **) &echo_msg, sizeof(*echo_msg));
        if (ret) {
            ZF_LOGF("Failed to request a buffer from tx_virtqueue");
        }

        *echo_msg = *msg;

        ret = camkes_virtqueue_driver_send_buffer(&tx_virtqueue, echo_msg, sizeof(*echo_msg));
        ZF_LOGF_IF(ret != 0, "Failed to add request to TX virtqueue");
        camkes_virtqueue_buffer_free(&rx_virtqueue, msg);
    }

    //trace_extra_point_end(2 + 2, 1);
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

    /*
    ret = trace_extra_point_register_name(2 + 1, "handle_udp_sent");
    ZF_LOGF_IF(ret, "Failed to register extra trace point %d", 2 + 1);

    ret = trace_extra_point_register_name(2 + 2, "handle_udp_received");
    ZF_LOGF_IF(ret, "Failed to register extra trace point %d", 2 + 2);
    */

    for (int i = 0; i < NUM_UDP_BUFS; i++) {
        tx_msg_t *msg = NULL;
        ret = camkes_virtqueue_buffer_alloc(&rx_virtqueue, (void **) &msg, sizeof(tx_msg_t));
        if (ret) {
            printf("Allocated %d bufs\n", i);
            break;
        }
        //tx_msg_t *buf = ps_dma_alloc(&io_ops->dma_manager, BUF_SIZE, 4, 1, PS_MEM_NORMAL);
        //ZF_LOGF_IF(buf == NULL, "Failed to allocate UDP DMA memory");
        memset(msg, 0, sizeof(tx_msg_t));
        msg->socket_fd = udp_socket;
        msg->total_len = UDP_READ_SIZE;
        msg->done_len = 0;
        msg->client_cookie = (void *) UDP_SOCKETS_ASYNC_ID;

        ret = camkes_virtqueue_driver_send_buffer(&rx_virtqueue, msg, sizeof(tx_msg_t));
        ZF_LOGF_IF(ret != 0, "Failed to add request to RX virtqueue");

        /*
        virtqueue_ring_object_t handle;
        virtqueue_init_ring_object(&handle);

        if (!virtqueue_add_available_buf(&rx_virtqueue, &handle, ENCODE_DMA_ADDRESS(buf),
                                         sizeof(*buf), VQ_RW)) {
            ZF_LOGF("Failed to add the UDP DMA buf to the RX virtqueue");
        }
        */
    }

    return 0;
}

CAMKES_POST_INIT_MODULE_DEFINE(setup_udp, setup_udp_echo_socket);
