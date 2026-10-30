/* badge_secsea © 2025 by Hack In Provence is licensed under
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license,
 * visit https://creativecommons.org/licenses/by-nc-sa/4.0/ */

/* The link with the ESP32-CAM module (camport.h), for the camera (camera.c) and the remote control (webremote.c) */

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#include "camport.h"
#include "pinouts.h"

/* Where the module may be plugged: the pins of a UART reaching a port of the badge. Searching, the badge says hello on
 * each one in turn and keeps the one that answers (the TX of the badge to U0R of the ESP32, its RX to U0T) */
static const struct {
    uint8_t uart, tx, rx;
    const char *where;
} PORTS[] = {
    {0, 16, 17, "UART0 GPIO16/17"},  /* The I2C0 pins of the SAO port (SDA = TX, SCL = RX) */
    {1, 20, 21, "UART1 GPIO20/21"},  /* The two GPIO of an SAO port */
    {0, BADGE_UART0_TX, BADGE_UART0_RX, "UART0 GPIO12/13"},  /* The 2 x 6 port J2 of the V1.1 board */
};
#define N_PORTS ((int)(sizeof(PORTS) / sizeof(PORTS[0])))
static int port = 0;
#define CAM_UART (PORTS[port].uart ? uart1 : uart0)
#define CAM_IRQ (PORTS[port].uart ? UART1_IRQ : UART0_IRQ)
#define RING_SIZE 3072  /* ~33 ms of the UART at 921600 bauds: more than the longest pause of the main loop */
#define HELLO_MS 1000
#define KEEP_ALIVE_MS 1500  /* Nothing received for this long: a packet that the module acknowledges */
#define LINK_LOST_MS 4000
#define PENDING 6
#define TRIES 10  /* Sent again this many times (every CAMLINK_ACK_MS), then given up */
#define SMALL_RX 96  /* The receiver without the camera page: the keys, the state of the access point */

/* ------ The UART: an interrupt fills the ring ------ */

static uint8_t ring[RING_SIZE];
static volatile uint16_t ring_head = 0, ring_tail = 0;
static volatile uint32_t ring_lost = 0;

static void uart_irq(void) {
    while (uart_is_readable(CAM_UART)) {
        uint8_t c = uart_getc(CAM_UART);
        uint16_t next = (ring_head + 1) % RING_SIZE;
        if (next == ring_tail)
            ++ring_lost;
        else {
            ring[ring_head] = c;
            ring_head = next;
        }
    }
}

static bool is_open = false;

static void link_open(void) {
    uart_init(CAM_UART, CAMLINK_BAUD);
    gpio_set_function(PORTS[port].tx, GPIO_FUNC_UART);
    gpio_set_function(PORTS[port].rx, GPIO_FUNC_UART);
    gpio_pull_up(PORTS[port].rx);  /* No module: no noise on the line */
    uart_set_fifo_enabled(CAM_UART, true);
    ring_head = ring_tail = 0;
    irq_set_exclusive_handler(CAM_IRQ, uart_irq);
    irq_set_enabled(CAM_IRQ, true);
    uart_set_irq_enables(CAM_UART, true, false);
    is_open = true;
}

static void link_close(void) {
    if (! is_open)
        return;
    uart_set_irq_enables(CAM_UART, false, false);
    irq_set_enabled(CAM_IRQ, false);
    irq_remove_handler(CAM_IRQ, uart_irq);
    uart_deinit(CAM_UART);
    gpio_disable_pulls(PORTS[port].rx);
    gpio_set_function(PORTS[port].tx, GPIO_FUNC_NULL);
    gpio_set_function(PORTS[port].rx, GPIO_FUNC_NULL);
    is_open = false;
}

/* ------ The state ------ */

static struct {
    bool on;
    camport_handler_t handler;
    void (*ready)(void);
} users[CAMPORT_USERS];

static uint8_t small_buf[SMALL_RX];
static camlink_rx_t rx;
static uint8_t *big_buf = NULL;
static size_t big_cap = 0;

static camport_state_t state = CAMPORT_OFF;
static char module_name[32];
static int module_proto = 0;
static absolute_time_t hello_ts, last_rx_ts, last_tx_ts;
static uint8_t out_seq = 0;
static int last_in_seq = -1;  /* The last data packet handled: sent again (its ACK lost), only acknowledged */

static struct {
    uint8_t type, seq, small_len, small[CAMPORT_SMALL];
    const uint8_t *ext;
    uint16_t ext_len;
    absolute_time_t ts;
    int tries;
} pending[PENDING];
static int n_pending = 0;

static void uart_out(const uint8_t *data, size_t len, void *ctx) {
    (void)ctx;
    uart_write_blocking(CAM_UART, data, len);
}

static void send_packet(uint8_t type, uint8_t seq, const uint8_t *head, size_t head_len, const uint8_t *body,
                        size_t body_len) {
    if (! is_open)
        return;
    camlink_write(type, seq, head, head_len, body, body_len, uart_out, NULL);
    last_tx_ts = get_absolute_time();
}

static bool any_user(void) {
    for (int u = 0; u < CAMPORT_USERS; ++u)
        if (users[u].on)
            return true;
    return false;
}

void camport_big_buffer(uint8_t *buf, size_t cap) {
    big_buf = buf;
    big_cap = cap;
    if (buf)
        camlink_rx_init(&rx, buf, cap);
    else
        camlink_rx_init(&rx, small_buf, sizeof(small_buf));
}

void camport_want(camport_user_t user, bool on, camport_handler_t handler, void (*ready)(void)) {
    bool was = any_user();
    users[user].on = on;
    users[user].handler = on ? handler : NULL;
    users[user].ready = on ? ready : NULL;
    if (on && ! was) {
        if (! big_buf)
            camlink_rx_init(&rx, small_buf, sizeof(small_buf));
        n_pending = 0;
        last_in_seq = -1;
        module_name[0] = 0;
        module_proto = 0;
        state = CAMPORT_SEARCHING;
        hello_ts = nil_time;
        last_rx_ts = last_tx_ts = get_absolute_time();
        link_open();
        printf("camport: on\n");
    } else if (on && state == CAMPORT_READY && ready) {
        ready();  /* Already found: this user can talk at once */
    } else if (! on && was && ! any_user()) {
        link_close();
        state = CAMPORT_OFF;
        n_pending = 0;
        printf("camport: off\n");
    }
}

camport_state_t camport_state(void) {
    return state;
}

const char *camport_module(void) {
    return module_name;
}

int camport_proto(void) {
    return module_proto;
}

const char *camport_where(void) {
    return PORTS[port].where;
}

bool camport_send(uint8_t type, const void *small, size_t small_len, const uint8_t *ext, size_t ext_len) {
    if (n_pending == PENDING || small_len > CAMPORT_SMALL || state == CAMPORT_OFF)
        return false;
    pending[n_pending].type = type;
    pending[n_pending].seq = ++out_seq;
    pending[n_pending].small_len = (uint8_t)small_len;
    if (small_len)
        memcpy(pending[n_pending].small, small, small_len);
    pending[n_pending].ext = ext;
    pending[n_pending].ext_len = (uint16_t)ext_len;
    pending[n_pending].tries = 0;
    pending[n_pending].ts = nil_time;  /* Sent at once (pump) */
    ++n_pending;
    return true;
}

int camport_pending(uint8_t type) {
    int n = 0;
    for (int i = 0; i < n_pending; ++i)
        if (! type || pending[i].type == type)
            ++n;
    return n;
}

void camport_send_once(uint8_t type, const void *payload, size_t len) {
    send_packet(type, ++out_seq, (const uint8_t *)payload, len, NULL, 0);
}

/* The first packet of the queue, sent (again) when its ACK did not come; one at a time, in order */
static void pump(absolute_time_t now) {
    if (! n_pending || state != CAMPORT_READY)
        return;
    if (pending[0].tries && absolute_time_diff_us(pending[0].ts, now) < CAMLINK_ACK_MS * 1000ll)
        return;
    if (++pending[0].tries > TRIES) {  /* The module does not take it: given up */
        printf("camport: packet %u given up\n", pending[0].type);
        memmove(&pending[0], &pending[1], sizeof(pending[0]) * --n_pending);
        return;
    }
    send_packet(pending[0].type, pending[0].seq, pending[0].small, pending[0].small_len, pending[0].ext,
                pending[0].ext_len);
    pending[0].ts = now;
}

static void acked(uint8_t seq, absolute_time_t now) {
    if (n_pending && pending[0].seq == seq) {
        memmove(&pending[0], &pending[1], sizeof(pending[0]) * --n_pending);
        pump(now);  /* The next one at once */
    }
}

/* The users of each type of packet of the module */
static camport_user_t user_of(uint8_t type) {
    return type == CL_KEY || type == CL_WEB_STATE ? CAMPORT_WEB : CAMPORT_CAMERA;
}

static void handle(absolute_time_t now) {
    last_rx_ts = now;
    switch (rx.type) {
    case CL_ACK:
        if (rx.len)
            acked(rx.payload[0], now);
        return;
    case CL_HELLO:
        /* The answer to our hello, or the module that started again: everybody sends its state again */
        snprintf(module_name, sizeof(module_name), "%.*s", (int)(rx.len > 1 ? rx.len - 1 : 0), rx.payload + 1);
        module_proto = rx.len ? rx.payload[0] : 0;
        printf("camera: module %s, protocol %d, on %s\n", module_name, module_proto, PORTS[port].where);
        state = CAMPORT_READY;
        n_pending = 0;
        last_in_seq = -1;
        for (int u = 0; u < CAMPORT_USERS; ++u)
            if (users[u].on && users[u].ready)
                users[u].ready();
        return;
    case CL_STATUS:  /* The camera (no camera), the update of the module (its result): everybody */
        for (int u = 0; u < CAMPORT_USERS; ++u)
            if (users[u].on && users[u].handler)
                users[u].handler(&rx, now);
        return;
    default:
        break;
    }
    /* The data: handled once, acknowledged (the module waits for it) */
    uint8_t seq = rx.seq;
    if (seq != last_in_seq) {
        last_in_seq = seq;
        camport_user_t u = user_of(rx.type);
        if (users[u].on && users[u].handler)
            users[u].handler(&rx, now);
    }
    send_packet(CL_ACK, 0, &seq, 1, NULL, 0);
}

void camport_task(absolute_time_t now) {
    if (state == CAMPORT_OFF)
        return;
    while (ring_tail != ring_head) {
        uint8_t c = ring[ring_tail];
        ring_tail = (ring_tail + 1) % RING_SIZE;
        if (camlink_feed(&rx, c))
            handle(now);
    }
    if (state == CAMPORT_SEARCHING && absolute_time_diff_us(hello_ts, now) > HELLO_MS * 1000ll) {
        hello_ts = now;  /* The module may start after the badge: asked again, on the next possible port */
        link_close();
        port = (port + 1) % N_PORTS;
        camlink_rx_init(&rx, rx.buf, rx.cap);
        link_open();
        uint8_t hello[] = {CAMLINK_PROTO, 'b', 'a', 'd', 'g', 'e'};
        send_packet(CL_HELLO, ++out_seq, hello, sizeof(hello), NULL, 0);
    }
    if (state == CAMPORT_READY) {
        if (absolute_time_diff_us(last_rx_ts, now) > LINK_LOST_MS * 1000ll) {
            state = CAMPORT_SEARCHING;  /* Unplugged, restarted: found again */
            module_name[0] = 0;
            n_pending = 0;
            printf("camport: module lost\n");
        } else if (! n_pending && absolute_time_diff_us(last_rx_ts, now) > KEEP_ALIVE_MS * 1000ll &&
                   absolute_time_diff_us(last_tx_ts, now) > KEEP_ALIVE_MS * 1000ll) {
            uint8_t nothing = 0;
            camport_send_once(CL_STATUS, &nothing, 1);  /* The module acknowledges any packet: still there */
        }
    }
    pump(now);
}
