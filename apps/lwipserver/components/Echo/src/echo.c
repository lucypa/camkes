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

seL4_Word echo_control_notification_badge(void);

camkes_msgqueue_receiver_t receiver;
virtqueue_driver_t tx_virtqueue;
virtqueue_driver_t rx_virtqueue;

static void handle_lwipserver_events(UNUSED seL4_Word badge, UNUSED void *cookie)
{
    //trace_extra_point_start(1);
    echo_control_poll_events(tcp_socket);
    if (peer_tcp_socket != -1) {
        echo_control_poll_events(peer_tcp_socket);
    }

    echo_control_poll_events(utilization_socket);
    if (peer_utilization_socket != -1) {
        echo_control_poll_events(peer_utilization_socket);
    }

    lwipserver_event_t event;
    while (1) {
        event = (lwipserver_event_t) {
            0
        };
        int error = camkes_msgqueue_get(&receiver, &event, sizeof(event));
        if (error) {
            break;
        }
        if (event.socket_fd == tcp_socket || event.socket_fd == peer_tcp_socket) {
            //trace_extra_point_end(1, 1);
            handle_tcp_event(&event);
            //trace_extra_point_start(1);
        } else if (event.socket_fd == utilization_socket ||
                   event.socket_fd == peer_utilization_socket) {
            //trace_extra_point_end(1, 1);
            handle_utilization_event(&event);
            //trace_extra_point_start(1);
        } else {
            ZF_LOGF("Invalid socket %d", event.socket_fd);
        }
    }
    //trace_extra_point_end(1, 1);
}

static tx_msg_t *get_msg_from_queue(virtqueue_driver_t *queue)
{
    //trace_extra_point_start(0);
    virtqueue_ring_object_t handle;
    uint32_t len;
    if (virtqueue_get_used_buf(queue, &handle, &len) == 0) {
        return NULL;
    }

    vq_flags_t flag;
    void *buf;
    int error = camkes_virtqueue_driver_gather_buffer(queue, &handle, &buf, &len, &flag);
    if (error) {
        ZF_LOGE("Failed to dequeue message from the virtqueue");
    }

    //trace_extra_point_end(0, 1);
    return buf;
}

static void handle_async_events(UNUSED seL4_Word badge, UNUSED void *cookie)
{
    //trace_extra_point_start(2);
    /* Handle responses from the TX virtqueue */
    while (true) {
        tx_msg_t *msg = get_msg_from_queue(&tx_virtqueue);
        if (!msg) {
            break;
        }
        if ((uintptr_t) msg->client_cookie == UDP_SOCKETS_ASYNC_ID) {
            handle_udp_socket_async_sent(msg);
        } else if ((uintptr_t) msg->client_cookie == TCP_SOCKETS_ASYNC_ID) {
            handle_tcp_socket_async_sent(msg);
        } else {
            ZF_LOGE("Message sent but bad socket: %d", msg->socket_fd);
        }
    }
    /* Handle responses from the RX virtqueue */
    while (true) {
        tx_msg_t *msg = get_msg_from_queue(&rx_virtqueue);
        if (!msg) {
            break;
        }
        if ((uintptr_t) msg->client_cookie == UDP_SOCKETS_ASYNC_ID) {
            handle_udp_socket_async_received(msg);
        } else if ((uintptr_t) msg->client_cookie == TCP_SOCKETS_ASYNC_ID) {
            handle_tcp_socket_async_received(msg);
        } else {
            ZF_LOGE("Message received but bad socket: %d", msg->socket_fd);
        }
    }
    //trace_extra_point_end(2, 1);
    tx_virtqueue.notify();
}

static int setup_echo_server(UNUSED ps_io_ops_t *io_ops)
{
    int error = camkes_msgqueue_receiver_init(0, &receiver);
    if (error) {
        assert(!"Failed to initialise receiver msgqueue");
    }

    seL4_Word tx_badge;
    seL4_Word rx_badge;

    error = camkes_virtqueue_driver_init_with_recv(&tx_virtqueue, camkes_virtqueue_get_id_from_name("echo_tx"), NULL, &tx_badge);
    ZF_LOGF_IF(error, "Failed to initialise the TX virtqueue");

    error = camkes_virtqueue_driver_init_with_recv(&rx_virtqueue, camkes_virtqueue_get_id_from_name("echo_rx"), NULL, &rx_badge);
    ZF_LOGF_IF(error, "Failed to initialise the RX virtqueue");

    error = single_threaded_component_register_handler(echo_control_notification_badge(),
                                                       "echo_sync_notification",
                                                       handle_lwipserver_events,
                                                       NULL);
    ZF_LOGF_IF(error, "Failed to register the sync event handler for Echo");

    error = single_threaded_component_register_handler(tx_badge, "echo_async_notification",
                                                       handle_async_events, NULL);
    ZF_LOGF_IF(error, "Failed to reigster the async event handler for Echo");

    /*
    error = trace_extra_point_register_name(0, "get_msg_from_queue");
    ZF_LOGF_IF(error, "Failed to register extra trace point 0");

    error = trace_extra_point_register_name(1, "handle_lwipserver_events");
    ZF_LOGF_IF(error, "Failed to register extra trace point 1");

    error = trace_extra_point_register_name(2, "handle_async_events");
    ZF_LOGF_IF(error, "Failed to register extra trace point 2");
    */

    printf("%s instance starting up, going to be listening on TCP port %d and receiving on UDP port %d\n",
           get_instance_name(), TCP_ECHO_PORT, UDP_ECHO_PORT);

    tx_virtqueue.notify();
    return 0;
}

CAMKES_POST_INIT_MODULE_DEFINE(setup_echo, setup_echo_server)
