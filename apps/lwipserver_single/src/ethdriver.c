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
#include <camkes/dma.h>
#include <camkes/io.h>
#include <camkes/interface_registration.h>
#include <camkes/irq.h>

#include <platsupport/io.h>
#include <platsupport/irq.h>
#include <utils/util.h>
#include <ethdrivers/raw.h>

#include <lwip/netif.h>
#include <lwip/init.h>
#include <netif/etharp.h>
#include <lwip/pbuf.h>
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <lwip/ip.h>
#include <lwip/dhcp.h>
#include <lwip/timeouts.h>

#include <echo/tuning_params.h>

#include <utils/fence.h>
#include <platsupport/arch/tsc.h>

struct eth_driver *eth_driver;
struct netif netif;

/*
 *Struct eth_buf contains a virtual address (buf) to use for memory operations
 *at the picoserver level and a physical address to be passed down to the
 *driver.
 */
typedef struct eth_buf {
    char *buf;
    uintptr_t phys;
    size_t len;
} eth_buf_t;

eth_buf_t rx_bufs[RX_BUFS];
eth_buf_t tx_bufs[TX_BUFS];

/* keeps track of the head of the queue */
int pending_rx_head;
/* keeps track of the tail of the queue */
int pending_rx_tail;

/*
 * this is a cyclic queue of RX buffers pending to be read by a client,
 * the head represents the first buffer in the queue, and the tail the last
 */
eth_buf_t *pending_rx[RX_BUFS];

/* keeps track of how many TX buffers are in use */
int num_tx;

/*
 * this represents the pool of buffers that can be used for TX,
 * this array is a sliding array in that num_tx acts a pointer to
 * separate between buffers that are in use and buffers that are
 * not in use. E.g. 'o' = free, 'x' = in use
 *  -------------------------------------
 *  | o | o | o | o | o | o | x | x | x |
 *  -------------------------------------
 *                          ^
 *                        num_tx
 */
eth_buf_t *tx_buf_pool[TX_BUFS];

static int num_rx_bufs;
static eth_buf_t *rx_buf_pool[RX_BUFS];

static void eth_tx_complete(void *iface, void *cookie)
{
    eth_buf_t *buf = (eth_buf_t *) cookie;
    tx_buf_pool[num_tx] = buf;
    num_tx++;
}

static uintptr_t eth_allocate_rx_buf(void *iface, size_t buf_size, void **cookie)
{
    if (buf_size > BUF_SIZE) {
        return 0;
    }
    if (num_rx_bufs == 0) {
        return 0;
    }
    num_rx_bufs--;
    *cookie = rx_buf_pool[num_rx_bufs];
    return rx_buf_pool[num_rx_bufs]->phys;
}

static void eth_rx_complete(void *iface, unsigned int num_bufs, void **cookies, unsigned int *lens)
{
    /* insert filtering here. currently everything just goes to one client */
    if (num_bufs != 1) {
        goto error;
    }
    eth_buf_t *curr_buf = cookies[0];
    if ((pending_rx_head + 1) % RX_BUFS == pending_rx_tail) {
        goto error;
    }
    curr_buf->len = lens[0];
    pending_rx[pending_rx_head] = curr_buf;
    pending_rx_head = (pending_rx_head + 1) % RX_BUFS;
    return;
error:
    /* abort and put all the bufs back */
    for (int i = 0; i < num_bufs; i++) {
        eth_buf_t *returned_buf = cookies[i];
        rx_buf_pool[num_rx_bufs] = returned_buf;
        num_rx_bufs++;
    }
}

static struct raw_iface_callbacks ethdriver_callbacks = {
    .tx_complete = eth_tx_complete,
    .rx_complete = eth_rx_complete,
    .allocate_rx_buf = eth_allocate_rx_buf
};

typedef struct lwip_custom_pbuf {
    struct pbuf_custom p;
    eth_buf_t *eth_buf;
} lwip_custom_pbuf_t;
LWIP_MEMPOOL_DECLARE(RX_POOL, RX_BUFS, sizeof(lwip_custom_pbuf_t), "Zero-copy RX pool");

static void lwip_free_buf(struct pbuf *buf)
{
    lwip_custom_pbuf_t *custom_pbuf = (lwip_custom_pbuf_t *) buf;

    rx_buf_pool[num_rx_bufs] = custom_pbuf->eth_buf;
    num_rx_bufs++;
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
}

/* Async driver will set a flag to signal that there is work to be done  */
static void lwip_eth_poll(struct netif *netif)
{
    int len;
    while (1) {
        int done;
        if (pending_rx_head == pending_rx_tail) {
            break;
        }

        eth_buf_t *rx = pending_rx[pending_rx_tail];
        lwip_custom_pbuf_t *custom_pbuf = (lwip_custom_pbuf_t *) LWIP_MEMPOOL_ALLOC(RX_POOL);
        ZF_LOGF_IF(custom_pbuf == NULL, "Failed to allocate custom pbuf");
        custom_pbuf->p.custom_free_function = lwip_free_buf;
        custom_pbuf->eth_buf = rx;
        struct pbuf *p = pbuf_alloced_custom(PBUF_RAW, rx->len, PBUF_REF, &custom_pbuf->p, rx->buf, BUF_SIZE);

        if (netif->input(p, netif) != ERR_OK) {
            ZF_LOGE("Failed to give pbuf to lwIP");
            pbuf_free(p);
            break;
        } else {
            pending_rx_tail = (pending_rx_tail + 1) % RX_BUFS;
        }
    }
}

static err_t lwip_eth_send(struct netif *netif, struct pbuf *p)
{
    if (p->tot_len > BUF_SIZE) {
        ZF_LOGF("len %hu is invalid in lwip_eth_send", p->tot_len);
    }

    if (num_tx == 0) {
        // No Ethernet frame buffers available
        return ERR_MEM;
    }

    num_tx--;
    eth_buf_t *tx_buf = tx_buf_pool[num_tx];

    /* copy the packet over */
    pbuf_copy_partial(p, tx_buf->buf, p->tot_len, 0);
    unsigned int len = p->tot_len;
    /* queue up transmit */
    int err = eth_driver->i_fn.raw_tx(eth_driver, 1, (uintptr_t *) & (tx_buf->phys),
                                      &len, tx_buf);

    switch (err) {
    case ETHIF_TX_FAILED:
        tx_buf_pool[num_tx] = tx_buf;
        num_tx++;
        return ERR_MEM;
    case ETHIF_TX_COMPLETE:
    case ETHIF_TX_ENQUEUED:
        break;
    }
    return ERR_OK;
}

static void tick_on_event()
{
    lwip_eth_poll(&netif);
}

static int hardware_interface_searcher(void *cookie, void *interface_instance, char **properties)
{
    eth_driver = interface_instance;
    return PS_INTERFACE_FOUND_MATCH;
}

static err_t ethernet_init(struct netif *netif)
{
    if (netif == NULL) {
        return ERR_ARG;
    }

    eth_driver->i_fn.get_mac(eth_driver, netif->hwaddr);
    netif->mtu = 1500;
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    netif->output = etharp_output;
    netif->linkoutput = lwip_eth_send;
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 1000000000);
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_IGMP;

    return ERR_OK;
}

static void netif_status_callback(struct netif *netif)
{
    if (dhcp_supplied_address(netif)) {
        printf("DHCP request finished, IP address for netif %s is: %s\n",
               netif->name, ip4addr_ntoa(netif_ip4_addr(netif)));
    }
}

int setup_e0(ps_io_ops_t *io_ops)
{
    int error = ps_interface_find(&io_ops->interface_registration_ops,
                                  PS_ETHERNET_INTERFACE, hardware_interface_searcher, NULL);
    if (error) {
        ZF_LOGE("Unable to find an ethernet device");
        return -1;

    }

    /* preallocate buffers */
    for (int i = 0; i < RX_BUFS; i++) {
        eth_buf_t *buf = &rx_bufs[i];
        buf->buf = ps_dma_alloc(&io_ops->dma_manager, BUF_SIZE, 64, 1, PS_MEM_NORMAL);
        if (!buf) {
            ZF_LOGE("Failed to allocate RX buffer.");
            return -1;

        }
        memset(buf->buf, 0, BUF_SIZE);
        buf->phys = ps_dma_pin(&io_ops->dma_manager, buf->buf, BUF_SIZE);
        if (!buf->phys) {
            ZF_LOGE("ps_dma_pin: Failed to return physical address.");
            return -1;

        }
        rx_buf_pool[num_rx_bufs] = buf;
        num_rx_bufs++;

    }

    for (int i = 0; i < TX_BUFS; i++) {
        eth_buf_t *buf = &tx_bufs[i];
        buf->buf = ps_dma_alloc(&io_ops->dma_manager, BUF_SIZE, 64, 1, PS_MEM_NORMAL);
        if (!buf) {
            ZF_LOGE("Failed to allocate TX buffer: %d.", i);
            return -1;

        }
        memset(buf->buf, 0, BUF_SIZE);
        buf->phys = ps_dma_pin(&io_ops->dma_manager, buf->buf, BUF_SIZE);
        if (!buf->phys) {
            ZF_LOGE("ps_dma_pin: Failed to return physical address.");
            return -1;

        }
        tx_buf_pool[num_tx] = buf;
        num_tx++;

    }

    /* Setup ethdriver callbacks and poll the driver so it can do any more init. */
    eth_driver->cb_cookie = NULL;
    eth_driver->i_cb = ethdriver_callbacks;
    eth_driver->i_fn.raw_poll(eth_driver);

    LWIP_MEMPOOL_INIT(RX_POOL);

    struct ip4_addr netmask, ipaddr, gw, multicast;
    ipaddr_aton("0.0.0.0", &gw);
    ipaddr_aton("0.0.0.0", &ipaddr);
    ipaddr_aton("0.0.0.0", &multicast);
    ipaddr_aton("255.255.255.0", &netmask);

    netif.name[0] = 'e';
    netif.name[1] = '0';

    netif_add(&netif, &ipaddr, &netmask, &gw, &netif,
              ethernet_init, ethernet_input);
    netif_set_default(&netif);

    single_threaded_component_register_handler(0, "sys_check_timeouts", tick_on_event, NULL);

    return 0;

}

CAMKES_POST_INIT_MODULE_DEFINE(install_eth_device, setup_e0);
