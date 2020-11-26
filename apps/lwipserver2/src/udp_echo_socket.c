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

#include <autoconf.h>
#include <stdbool.h>

#include <camkes.h>

#include <lwip/ip.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>

#include "ports.h"
#include <echo/tuning_params.h>

static char udp_data_packet[0x1000] ALIGN(0x1000);

static struct udp_pcb *udp_socket;

typedef struct lwip_custom_pbuf {
    struct pbuf_custom p;
    bool is_echo;
    void *dma_buf;
} lwip_custom_pbuf_t;

static void lwip_udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (p->flags & PBUF_FLAG_IS_CUSTOM) {
        lwip_custom_pbuf_t *custom_pbuf = (lwip_custom_pbuf_t *) p;
        custom_pbuf->is_echo = true;
    }
    //trace_extra_point_start(6);
    //trace_extra_point_start(7);
    err_t error = udp_sendto(pcb, p, addr, port);
    //trace_extra_point_end(7, 1);
    ZF_LOGF_IF(error, "Failed to send UDP packet through socket");
    //trace_extra_point_start(4);
    pbuf_free(p);
    //trace_extra_point_end(4, 1);
    //trace_extra_point_end(6, 1);
}

int setup_udp_socket(UNUSED ps_io_ops_t *io_ops)
{
    udp_socket = udp_new_ip_type(IPADDR_TYPE_V4);
    if (udp_socket == NULL) {
        ZF_LOGE("Failed to open a UDP socket");
        return -1;
    }
    udp_recv(udp_socket, lwip_udp_recv_callback, udp_socket);
    int error = udp_bind(udp_socket, IP_ANY_TYPE, UDP_ECHO_PORT);
    if (error) {
        ZF_LOGE("Failed to bind the UDP socket");
        return -1;
    }
    return 0;
}

CAMKES_POST_INIT_MODULE_DEFINE(setup_udp_socket_, setup_udp_socket);
