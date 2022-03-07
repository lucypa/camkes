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
#include <assert.h>
#include <stdint.h>
#include <string.h>

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
#include <lwip/sys.h>

#include <echo/tuning_params.h>

#include <utils/fence.h>

#define LINK_SPEED 1000000000 // Gigabit

struct eth_driver *eth_driver;
struct netif netif;
ps_io_ops_t *ops;

/*
 * Ethernet buffers
 * ================
 */

#define NUM_BUFFERS 512
#define BUFFER_SIZE 2048

/*
 * These structures track the buffers used to construct packets
 * sent via this network interface.
 *
 * As the interface is asynchronous, when a buffer is freed it isn't
 * returned to the pool until any outstanding asynchronous use
 * completes.
 */
typedef struct ethernet_buffer {
    /* The acutal underlying memory of the buffer */
    unsigned char *buffer;
    /* The physical address of the buffer */
    uintptr_t phys;
    /* The physical size of the buffer */
    size_t size;
    /* Whether the buffer has been allocated */
    bool allocated;
    /* Whether the buffer is in use by the ethernet device */
    bool in_async_use;
} ethernet_buffer_t;

/* Static pool of buffer metadata structures */
static ethernet_buffer_t buffers[NUM_BUFFERS];

/* Pool of free buffers */
static ethernet_buffer_t *free_buffers[NUM_BUFFERS];
static size_t num_free_buffers = 0;

/* Allocate a new buffer */
static inline ethernet_buffer_t *alloc_buffer(size_t length);

/*
 * Free a buffer
 *
 * If the buffer is currently in used, the free will be delayed until
 * the network operation is marked as complete.
 */
static inline void free_buffer(ethernet_buffer_t *buffer);

/*
 * Mark a buffer as in-use for an asynchronous network operation.
 *
 * The buffer must be presently allocated and not already marked used.
 */
static inline void mark_buffer_used(ethernet_buffer_t *buffer);

/* Remove the used marker after an asynchronous network operation has
 * completed. */
static inline void mark_buffer_unused(ethernet_buffer_t *buffer);

/*
 * Ethernet device
 * ===============
 */

/* Callback for completion of frame transfer */
static void tx_complete(void *iface, void *cookie);

/* Callback for frame receive */
static void rx_complete(
    void *iface,
    unsigned int num_bufs,
    void **cookies,
    unsigned int *lens
);

/* Callback for allocation request of frame for receive */
static uintptr_t allocate_rx_buf(
    void *iface,
    size_t buf_size,
    void **cookie
);

static struct raw_iface_callbacks ethdriver_callbacks = {
    .tx_complete = tx_complete,
    .rx_complete = rx_complete,
    .allocate_rx_buf = allocate_rx_buf
};

/*
 * Network interface
 * =================
 */

/* Custom buffers used for network interface */
typedef struct lwip_custom_pbuf {
    struct pbuf_custom custom;
    ethernet_buffer_t *buffer;
} lwip_custom_pbuf_t;
LWIP_MEMPOOL_DECLARE(
    RX_POOL,
    NUM_BUFFERS,
    sizeof(lwip_custom_pbuf_t),
    "Zero-copy RX pool"
);

/* Receive a buffer from the network and pass to LwIP */
static void interface_receive(ethernet_buffer_t *buffer, size_t length);

/* Create an LwIP buffer from an ethernet buffer */
static struct pbuf *create_interface_buffer(
    ethernet_buffer_t *buffer,
    size_t length
);

/* Free a buffer used by LwIP */
static void interface_free_buffer(struct pbuf *buf);

/* Initialisation for the LwIP interface */
static err_t interface_init(struct netif *netif);

/* Send an ethernet frame via the interface */
static err_t interface_eth_send(struct netif *netif, struct pbuf *p);

/*
 * Ethernet buffers
 * ================
 */

static inline ethernet_buffer_t *alloc_buffer(size_t length)
{
    if (num_free_buffers > 0) {
        num_free_buffers -= 1;
        ethernet_buffer_t *buffer =  free_buffers[num_free_buffers];

        if (buffer->size < length) {
            /* Requested size too large */
            num_free_buffers += 1;
            ZF_LOGE("Cannot alloc buffer... Requested size too large");
            return NULL;
        } else {
            buffer->allocated = true;
            return buffer;
        }
    } else {
        ZF_LOGE("Cannot alloc buffer... none available");
        return NULL;
    }
}

static inline void free_buffer(ethernet_buffer_t *buffer)
{
    assert(buffer != NULL);
    assert(buffer->allocated);
    assert(num_free_buffers <= NUM_BUFFERS);


    if (!buffer->in_async_use) {
        free_buffers[num_free_buffers] = buffer;
        num_free_buffers += 1;
    }

    buffer->allocated = false;
}

static inline void mark_buffer_used(ethernet_buffer_t *buffer)
{
    assert(buffer != NULL);
    assert(buffer->allocated);
    assert(!buffer->in_async_use);
    buffer->in_async_use = true;
}

static inline void mark_buffer_unused(ethernet_buffer_t *buffer)
{
    assert(buffer != NULL);
    assert(buffer->in_async_use);
    assert(num_free_buffers <= NUM_BUFFERS);

    if (!buffer->allocated) {
        free_buffers[num_free_buffers] = buffer;
        num_free_buffers += 1;
    }

    buffer->in_async_use = false;
}

/*
 * Ethernet device
 * ===============
 */

static void tx_complete(void *iface, void *cookie)
{
    mark_buffer_unused(cookie);
}

static void rx_complete(
    void *iface,
    unsigned int num_bufs,
    void **cookies,
    unsigned int *lens
) {
    
    for (int b = 0; b < num_bufs; b += 1) {
        ethernet_buffer_t *buffer = cookies[b];
        mark_buffer_unused(buffer);
        interface_receive(buffer, lens[b]);
    }
}

static uintptr_t allocate_rx_buf(
    void *iface,
    size_t buf_size,
    void **cookie
) {
    ethernet_buffer_t *buffer = alloc_buffer(buf_size);
    if (buffer == NULL) {
        return 0;
    }

    /* Buffer now used for receive */
    mark_buffer_used(buffer);

    /* Invalidate the memory */
    *cookie = buffer;
    ps_dma_cache_invalidate(
        &ops->dma_manager,
        buffer->buffer,
        buf_size
    );
    //*cookie = buffer;
    return buffer->phys;
}

/*
 * Network interface
 * =================
 */

static void interface_receive(ethernet_buffer_t *buffer, size_t length)
{   
    /* Invalidate the memory. This second invalidate may or may not be
    necessary. On arm, stale data comes up without this, and I suspect this
    is because of the prefetcher working in the background during dma. */
    ps_dma_cache_invalidate(
        &ops->dma_manager,
        buffer->buffer,
        length
    );
    /* If we wanted to add a queue, we'd do so in here */
    struct pbuf *p = create_interface_buffer(buffer, length);
    if (netif.input(p, &netif) != ERR_OK) {
        /* If it is successfully received, the receiver controls whether
         * or not it gets freed. */
        ZF_LOGE("netif.input() != ERR_OK");
        pbuf_free(p);
    }
}

static struct pbuf *create_interface_buffer(
    ethernet_buffer_t *buffer,
    size_t length
) {
    lwip_custom_pbuf_t *custom_pbuf =
        (lwip_custom_pbuf_t *) LWIP_MEMPOOL_ALLOC(RX_POOL);

    custom_pbuf->buffer = buffer;

    custom_pbuf->custom.custom_free_function = interface_free_buffer;
    
    return pbuf_alloced_custom(
        PBUF_RAW,
        length,
        PBUF_REF,
        &custom_pbuf->custom,
        buffer->buffer,
        buffer->size
    );
}

static void interface_free_buffer(struct pbuf *buf)
{
    SYS_ARCH_DECL_PROTECT(old_level);

    lwip_custom_pbuf_t *custom_pbuf = (lwip_custom_pbuf_t *) buf;

    SYS_ARCH_PROTECT(old_level);
    free_buffer(custom_pbuf->buffer);
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
    SYS_ARCH_UNPROTECT(old_level);
}

static err_t interface_init(struct netif *netif)
{
    if (netif == NULL) {
        return ERR_ARG;
    }

    eth_driver->i_fn.get_mac(eth_driver, netif->hwaddr);
    netif->mtu = 1500;
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    netif->output = etharp_output;
    netif->linkoutput = interface_eth_send;
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED);
    netif->flags =
        ( NETIF_FLAG_BROADCAST
        | NETIF_FLAG_ETHARP
        | NETIF_FLAG_LINK_UP
        | NETIF_FLAG_IGMP
        );

    return ERR_OK;
}

static err_t interface_eth_send(struct netif *netif, struct pbuf *p)
{
    err_t ret = ERR_OK;

    if (p->tot_len > BUF_SIZE) {
        ZF_LOGF("len %hu is invalid in lwip_eth_send", p->tot_len);
        return ERR_MEM;
    }

    /*
     * If the largest pbuf is a custom pbuf and the remaining pbufs can
     * be packed around it into the allocation, they are copied into the
     * ethernet frame, otherwise we allocate a new buffer and copy
     * everything.
     */
     
    ethernet_buffer_t *buffer = NULL;
    unsigned char *frame = NULL;

    /*
     * We need to allocate a new buffer if a suitable one wasn't found.
     */
    bool buffer_allocated = false;
    if (buffer == NULL) {
        buffer = alloc_buffer(p->tot_len);
        if (buffer == NULL) {
            ZF_LOGF("Out of ethernet memory");
            return ERR_MEM;
        }
        frame = buffer->buffer;
        buffer_allocated = true;
    }

    /* Copy all buffers that need to be copied */
    unsigned int copied = 0;
    for (struct pbuf *curr = p; curr != NULL; curr = curr->next) {
        unsigned char *buffer_dest = &frame[copied];
        if ((uintptr_t)buffer_dest != (uintptr_t)curr->payload) {
            /* Don't copy memory back into the same location */
            memcpy(buffer_dest, curr->payload, curr->len);
        }
        copied += curr->len;
    }

    ps_dma_cache_clean(&ops->dma_manager, frame, copied);

    mark_buffer_used(buffer);
    uintptr_t phys = buffer->phys;
    int err = eth_driver->i_fn.raw_tx(
        eth_driver,
        1,
        &phys,
        &copied,
        buffer
    );

    switch (err) {
    case ETHIF_TX_FAILED:
        tx_complete(NULL, buffer);
        ret = ERR_MEM;
        break;
    case ETHIF_TX_COMPLETE:
        tx_complete(NULL, buffer);
        break;
    case ETHIF_TX_ENQUEUED:
        break;
    }

    if (buffer_allocated) {
        free_buffer(buffer);
    }
    return ret;
}

static int hardware_interface_searcher(void *cookie, void *interface_instance, char **properties)
{
    eth_driver = interface_instance;
    return PS_INTERFACE_FOUND_MATCH;
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
    ops = io_ops;

    int error = ps_interface_find(&io_ops->interface_registration_ops,
                                  PS_ETHERNET_INTERFACE, hardware_interface_searcher, NULL);
    if (error) {
        ZF_LOGE("Unable to find an ethernet device");
        return -1;
    }

    /* preallocate buffers */
    LWIP_MEMPOOL_INIT(RX_POOL);
    for (int b = 0; b < NUM_BUFFERS; b++) {
        ethernet_buffer_t *buffer = &buffers[b];
        buffer->buffer = ps_dma_alloc(
            &io_ops->dma_manager,
            BUFFER_SIZE,
            64,
            1,
            PS_MEM_NORMAL
        );
        if (!buffer->buffer) {
            ZF_LOGE("Failed to allocate buffer.");
            return -1;

        }
        buffer->phys = ps_dma_pin(
            &ops->dma_manager,
            buffer->buffer,
            BUFFER_SIZE
        );
        if (!buffer->buffer) {
            ZF_LOGE("Failed to pin buffer.");
            return -1;

        }
        buffer->size = BUFFER_SIZE;
        memset(buffer->buffer, 0, BUFFER_SIZE);
        free_buffers[num_free_buffers] = buffer;
        num_free_buffers++;

    }

    /* Setup ethdriver callbacks and poll the driver so it can do any more init. */
    eth_driver->cb_cookie = NULL;
    eth_driver->i_cb = ethdriver_callbacks;
    eth_driver->i_fn.raw_poll(eth_driver);


    /* Configure the ethernet device */
    struct ip4_addr netmask, ipaddr, gw, multicast;
    ipaddr_aton("10.13.0.1", &gw);
    ipaddr_aton("0.0.0.0", &ipaddr);
    ipaddr_aton("0.0.0.0", &multicast);
    ipaddr_aton("255.255.255.0", &netmask);

    netif.name[0] = 'e';
    netif.name[1] = '0';

    netif_add(&netif, &ipaddr, &netmask, &gw, &netif,
              interface_init, ethernet_input);
    netif_set_status_callback(&netif, netif_status_callback);
    netif_set_default(&netif);

    return 0;
}

CAMKES_POST_INIT_MODULE_DEFINE(install_eth_device, setup_e0);
