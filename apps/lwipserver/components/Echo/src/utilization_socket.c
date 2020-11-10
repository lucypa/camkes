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

// #include "client.h"

// int utilization_socket = -1;
// int peer_utilization_socket = -1;

// /* This file implements a TCP based utilization measurment process that starts
//  * and stops utilization measurements based on a client's requests.
//  * The protocol used to communicate is as follows:
//  * - Client connects
//  * - Server sends: 100 IPBENCH V1.0\n
//  * - Client sends: HELLO\n
//  * - Server sends: 200 OK (Ready to go)\n
//  * - Client sends: LOAD cpu_target_lukem\n
//  * - Server sends: 200 OK\n
//  * - Client sends: SETUP args::""\n
//  * - Server sends: 200 OK\n
//  * - Client sends: START\n
//  * - Client sends: STOP\n
//  * - Server sends: 220 VALID DATA (Data to follow)\n
//  *                                Content-length: %d\n
//  *                                ${content}\n
//  * - Server closes socket.
//  *
//  * It is also possible for client to send QUIT\n during operation.
//  *
//  * The server starts recording utilization stats when it receives START and
//  * finishes recording when it receives STOP.
//  *
//  * Only one client can be connected.
//  */

// #define WHOAMI "100 IPBENCH V1.0\n"
// #define HELLO "HELLO\n"
// #define OK_READY "200 OK (Ready to go)\n"
// #define LOAD "LOAD cpu_target_lukem\n"
// #define OK "200 OK\n"
// #define SETUP "SETUP args::\"\"\n"
// #define START "START\n"
// #define STOP "STOP\n"
// #define QUIT "QUIT\n"
// #define RESPONSE "220 VALID DATA (Data to follow)\n"    \
//     "Content-length: %d\n"                              \
//     "%s\n"
// #define IDLE_FORMAT ",%ld,%ld"
// #define msg_match(msg, match) (strncmp(msg, match, strlen(match))==0)

// static int handle_utilization_peer_data(void)
// {
//     int ret = echo_recv_read(peer_utilization_socket, 0x1000, 0);
//     ZF_LOGF_IF(ret < 0, "Failed to read data from utilization socket");

//     if (msg_match(echo_recv_buf, HELLO)) {
//         memcpy(echo_send_buf, OK_READY, strlen(OK_READY));
//         ret = echo_send_write(peer_utilization_socket, strlen(OK_READY), 0);
//         ZF_LOGF_IF(ret < strlen(OK_READY), "Failed to send OK_READY message");
//     } else if (msg_match(echo_recv_buf, LOAD)) {
//         memcpy(echo_send_buf, OK, strlen(OK));
//         ret = echo_send_write(peer_utilization_socket, strlen(OK), 0);
//         ZF_LOGF_IF(ret < strlen(OK), "Failed to send reply for LOAD message");
//     } else if (msg_match(echo_recv_buf, SETUP)) {
//         memcpy(echo_send_buf, OK, strlen(OK));
//         ret = echo_send_write(peer_utilization_socket, strlen(OK), 0);
//         ZF_LOGF_IF(ret < strlen(OK), "Failed to send reply for SETUP message");
//     } else if (msg_match(echo_recv_buf, START)) {
//         idle_start();
//     } else if (msg_match(echo_recv_buf, STOP)) {
//         uint64_t total, kernel, idle;
//         idle_stop(&total, &kernel, &idle);
//         char *util_msg;
//         int len = asprintf(&util_msg, IDLE_FORMAT, idle, total);
//         if (len == -1) {
//             ZF_LOGE("Failed to format the utilisation message for ipbench");
//         } else {
//             len = snprintf(echo_send_buf, 0x1000, RESPONSE, len + 1, util_msg);
//             if (len == -1) {
//                 ZF_LOGE("Failed to format the response message for ipbench");
//             } else {
//                 ret = echo_send_write(peer_utilization_socket, len, 0);
//                 ZF_LOGF_IF(ret < len, "Failed to send reply for STOP message");
//             }
//             free(util_msg);
//         }
//         echo_control_shutdown(peer_utilization_socket, 0, 1);
//     } else if (msg_match(echo_recv_buf, QUIT)) {
//         /* Do nothing for now */
//     } else {
//         printf("Received a message that we can't handle %s\n", echo_recv_buf);
//     }

//     return 0;
// }

// int handle_utilization_event(lwipserver_event_t *event)
// {
//     int error = 0;
//     ip_addr_t peer_addr;
//     char ip_string[IP4ADDR_STRLEN_MAX + 1] = {0};
//     uint16_t peer_port;

//     if (event->type & LWIPSERVER_PEER_CLOSED) {
//         assert(event->socket_fd == peer_utilization_socket);
//         error = echo_control_close(peer_utilization_socket);
//         ZF_LOGF_IF(error, "Failed to close the peer utilization socket");
//         peer_utilization_socket = -1;
//     }

//     if (event->type & LWIPSERVER_PEER_AVAIL) {
//         assert(event->socket_fd == utilization_socket);
//         error = echo_control_accept(utilization_socket, &peer_addr, &peer_port,
//                                     &peer_utilization_socket);
//         ZF_LOGF_IF(error, "Failed to accept the new peer");
//         ip4addr_ntoa_r(&peer_addr, ip_string, IP4ADDR_STRLEN_MAX);
//         printf("Utilization peer connected from %s:%hu\n", ip_string, peer_port);
//         memcpy(echo_send_buf, WHOAMI, strlen(WHOAMI));
//         error = echo_send_write(peer_utilization_socket, strlen(WHOAMI), 0);
//         ZF_LOGF_IF(error < 0, "Failed to send WHOAMI message");
//     }

//     if (event->type & LWIPSERVER_DATA_AVAIL) {
//         return handle_utilization_peer_data();
//     }

//     return 0;
// }

// static int setup_utilization_socket(UNUSED ps_io_ops_t *io_ops)
// {
//     utilization_socket = echo_control_open(false);
//     if (utilization_socket == -1) {
//         ZF_LOGF("Failed to open the utilization socket");
//     }

//     int error = echo_control_bind(utilization_socket, *IP_ANY_TYPE, UTILIZATION_PORT);
//     ZF_LOGF_IF(error, "Failed to bind the utilization socket");

//     error = echo_control_listen(utilization_socket, 1);
//     ZF_LOGF_IF(error, "Failed to list on the utilization socket");

//     return 0;
// }

// CAMKES_POST_INIT_MODULE_DEFINE(setup_utilization, setup_utilization_socket);
