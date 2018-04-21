#include <stdio.h>
#include "ringbuffer.h"
#include "esp8266.h"

#define ESP8266_CONNECT_TIMEOUT 15000
#define ESP8266_SEND_TIMEOUT    500
#define ESP8266_RECV_TIMEOUT    500
#define ESP8266_MISC_TIMEOUT    500

#define ESP8266_SOCKET_COUNT    5

static int connect_error;
static bool fail;

static ringbuffer ringbuf;
static atcmd at;

static int socket_open[ESP8266_SOCKET_COUNT];
static unsigned long id_flags = 0;

static void packet_handler(void);
static void connect_error_handler(void);
static void socket0_closed_handler(void);
static void socket1_closed_handler(void);
static void socket2_closed_handler(void);
static void socket3_closed_handler(void);
static void socket4_closed_handler(void);

static int get_fd(void)
{
    for (int i = 0; i < ESP8266_SOCKET_COUNT; i++) {
        if (!(id_flags & (1 << i))) {
            id_flags |= (1 << i);
            return i;
        }
    }
    return -1;
}

static void put_fd(int fd)
{
    if (fd >= 0 && fd < ESP8266_SOCKET_COUNT) {
        id_flags &= ~(1 << fd);
    }
}

void esp8266_init(atcmd_ops *ops)
{
    static bool inited = false;
    static char ringbuf_buf[2048]; 
    static char at_buf[256];

    if (!inited) {

        atcmd_init(&at, ops, at_buf, sizeof(at_buf));

        ringbuffer_init(&ringbuf, ringbuf_buf, sizeof(ringbuf_buf));

        atcmd_add_handler(&at, "+IPD", packet_handler);
        atcmd_add_handler(&at, "+CWJAP:", connect_error_handler);
        atcmd_add_handler(&at, "0,CLOSED", socket0_closed_handler);
        atcmd_add_handler(&at, "1,CLOSED", socket1_closed_handler);
        atcmd_add_handler(&at, "2,CLOSED", socket2_closed_handler);
        atcmd_add_handler(&at, "3,CLOSED", socket3_closed_handler);
        atcmd_add_handler(&at, "4,CLOSED", socket4_closed_handler);

        inited = true;
    }
}

static void packet_handler(void)
{
    int fd;
    int amount;

    if (!atcmd_recv(&at, ",%d,%d:", &fd, &amount)) {
        return;
    }

    for (int i = 0; i < amount; i++) {
        char ch;
        if (atcmd_read(&at, &ch, 1) != 1) {
            break;
        }
        ringbuffer_put(&ringbuf, ch);
    }
}

static void socket0_closed_handler(void)
{
    socket_open[0] = 0;
    put_fd(0);
}

static void socket1_closed_handler(void)
{
    socket_open[1] = 0;
    put_fd(1);
}

static void socket2_closed_handler(void)
{
    socket_open[2] = 0;
    put_fd(2);
}

static void socket3_closed_handler(void)
{
    socket_open[3] = 0;
    put_fd(3);
}

static void socket4_closed_handler(void)
{
    socket_open[4] = 0;
    put_fd(4);
}

bool esp8266_reset(void)
{
    bool done;

    atcmd_set_timeout(&at, ESP8266_CONNECT_TIMEOUT);

    for (int i = 0; i < 2; i++) {
        done = atcmd_send(&at, "AT+RST") &&
            atcmd_recv(&at, "OK\n") &&
            atcmd_recv(&at, "ready");
        if (done){
            break;
        }
    }

    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);
    return done;
}

int esp8266_get_firmware_version(void)
{
    int version;

    if (atcmd_send(&at, "AT+GMR") &&
        atcmd_recv(&at, "SDK version:%d", &version) &&
        atcmd_recv(&at, "OK\n"))
    {
        return version;
    }

    return -1;
}

int esp8266_get_default_wifi_mode(void)
{
    int8_t mode;

    if (atcmd_send(&at, "AT+CWMODE_DEF?") &&
        atcmd_recv(&at, "+CWMODE_DEF:%hhd", &mode) &&
        atcmd_recv(&at, "OK\n"))
    {
        return mode;
    }

    return 0;
}

bool esp8266_set_default_wifi_mode(const int8_t mode)
{
    return atcmd_send(&at, "AT+CWMODE_DEF=%hhd", mode) && atcmd_recv(&at, "OK\n");
}

const char *esp8266_get_ipaddress(void)
{
    static char ip_buffer[16];
    char *ip = NULL;

    atcmd_set_timeout(&at, ESP8266_CONNECT_TIMEOUT);

    if (atcmd_send(&at, "AT+CIFSR") &&
        atcmd_recv(&at, "+CIFSR:STAIP,\"%15[^\"]\"", ip_buffer) &&
        atcmd_recv(&at, "OK\n"))
    {
        ip = ip_buffer;
    }

    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);
    return ip;
}

const char *esp8266_get_macaddress(void)
{
    static char _mac_buffer[18];
    if (!(atcmd_send(&at, "AT+CIFSR") &&
          atcmd_recv(&at, "+CIFSR:STAMAC,\"%17[^\"]\"", _mac_buffer) &&
          atcmd_recv(&at, "OK\n")))
    {
        return 0;
    }
    return _mac_buffer;
}

const char *esp8266_get_gateway(void)
{
    static char _gateway_buffer[16];
    if (!(atcmd_send(&at, "AT+CIPSTA_CUR?") &&
          atcmd_recv(&at, "+CIPSTA_CUR:gateway:\"%15[^\"]\"", _gateway_buffer) &&
          atcmd_recv(&at, "OK\n")))
    {
        return 0;
    }
    return _gateway_buffer;
}

const char *esp8266_get_netmask(void)
{
    static char _netmask_buffer[16];
    if (!(atcmd_send(&at, "AT+CIPSTA_CUR?") &&
          atcmd_recv(&at, "+CIPSTA_CUR:netmask:\"%15[^\"]\"", _netmask_buffer) &&
          atcmd_recv(&at, "OK\n")))
    {
        return 0;
    }
    return _netmask_buffer;
}

int esp8266_get_rssi(void)
{
    int8_t rssi;
    char bssid[18];

    atcmd_set_timeout(&at, ESP8266_CONNECT_TIMEOUT);
    if (!(atcmd_send(&at, "AT+CWJAP_CUR?") &&
          atcmd_recv(&at, "+CWJAP_CUR:\"%*[^\"]\",\"%17[^\"]\"", bssid) &&
          atcmd_recv(&at, "OK\n")))
    {
        return 0;
    }
    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);

    atcmd_set_timeout(&at, ESP8266_CONNECT_TIMEOUT);
    if (!(atcmd_send(&at, "AT+CWLAP=\"\",\"%s\",", bssid) &&
          atcmd_recv(&at, "+CWLAP:(%*d,\"%*[^\"]\",%hhd,", &rssi) &&
          atcmd_recv(&at, "OK\n")))
    {
        return 0;
    }
    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);

    return rssi;
}

int esp8266_startup(int mode)
{
    if (mode != ESP8266_WIFIMODE_STATION &&
        mode != ESP8266_WIFIMODE_SOFTAP &&
        mode != ESP8266_WIFIMODE_STATION_SOFTAP)
    {
        return false;
    }

    atcmd_set_timeout(&at, ESP8266_CONNECT_TIMEOUT);

    bool done = atcmd_send(&at, "AT+CWMODE_CUR=%d", mode) &&
        atcmd_recv(&at, "OK\n") &&
        atcmd_send(&at, "AT+CIPMUX=1") &&
        atcmd_recv(&at, "OK\n");

    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);
    return done;
}

bool esp8266_dhcp(bool enabled, int mode)
{
    if (mode < 0 || mode > 2)
    {
        return false;
    }

    return atcmd_send(&at, "AT+CWDHCP_CUR=%d,%d", mode, enabled ? 1 : 0)
        && atcmd_recv(&at, "OK\n");
}

static void connect_error_handler(void)
{
    fail = false;
    connect_error = 0;

    if (atcmd_recv(&at, "%d", &connect_error) &&
        atcmd_recv(&at, "FAIL")) {
        fail = true;
        atcmd_abort(&at);
    }
}

int esp8266_connect(const char *ap, const char *password)
{
    nsapi_error_t ret = NSAPI_ERROR_OK;

    atcmd_set_timeout(&at, ESP8266_CONNECT_TIMEOUT);

    atcmd_send(&at, "AT+CWJAP_CUR=\"%s\",\"%s\"", ap, password);

    if (!atcmd_recv(&at, "OK\n")) {
        if (fail) {
            if (connect_error == 1)
                ret = NSAPI_ERROR_CONNECTION_TIMEOUT;
            else if (connect_error == 2)
                ret = NSAPI_ERROR_AUTH_FAILURE;
            else if (connect_error == 3)
                ret = NSAPI_ERROR_NO_SSID;
            else
                ret = NSAPI_ERROR_NO_CONNECTION;

            fail = false;
            connect_error = 0;
        }
    }

    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);
    return ret;
}

int esp8266_open_tcp(const char *addr, int port, int keepalive)
{
    int fd = get_fd();
    if (fd < 0 || socket_open[fd]) {
        return -1;
    }

    bool done = false;
    static const char *type = "TCP";
    if (keepalive) {
        done = atcmd_send(&at, "AT+CIPSTART=%d,\"%s\",\"%s\",%d,%d",
                          fd, type, addr, port, keepalive) &&
            atcmd_recv(&at, "OK\n");
    } else {
        done = atcmd_send(&at, "AT+CIPSTART=%d,\"%s\",\"%s\",%d",
                          fd, type, addr, port) &&
            atcmd_recv(&at, "OK\n");
    }

    if (done) {
        socket_open[fd] = 1;
    } else {
        put_fd(fd);
        fd = -1;
    }
    return fd;
}

static int _send_tcp(int fd, const void *data, unsigned int amount)
{
    int rc = NSAPI_ERROR_DEVICE_ERROR;

    atcmd_set_timeout(&at, ESP8266_SEND_TIMEOUT);

    bool done = atcmd_send(&at, "AT+CIPSEND=%d,%lu", fd, amount) && atcmd_recv(&at, ">");
    if (done) {
        rc = atcmd_write(&at, (char *)data, (int)amount);
        atcmd_set_timeout(&at, ESP8266_RECV_TIMEOUT);
        atcmd_process_oob(&at); // Poll for inbound packets
        atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);
    }

    atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);

    return rc;
}

int esp8266_send_tcp(int fd, const void *data, unsigned int amount)
{
    if (socket_open[fd] == 0) {
        return NSAPI_ERROR_CONNECTION_LOST;
    }

    int rc = NSAPI_ERROR_DEVICE_ERROR;

    for (int i = 0; i < 2; i++) {
        rc = _send_tcp(fd, data, amount);
        if (rc >= 0) {
            break;
        }
    }
    return rc;
}

int esp8266_recv_tcp(int fd, void *data, unsigned int amount)
{
    if (socket_open[fd] == 0) {
        return NSAPI_ERROR_CONNECTION_LOST;
    }

    unsigned int len = ringbuffer_length(&ringbuf);
    if (!len) {
        atcmd_set_timeout(&at, ESP8266_RECV_TIMEOUT);
        atcmd_process_oob(&at); // Poll for inbound packets
        atcmd_set_timeout(&at, ESP8266_MISC_TIMEOUT);
        return 0;
    }

    if (len > amount) {
        len = amount;
    }
    int count = len;
    char *p = data;
    while (count--) {
        ringbuffer_get(&ringbuf, p++);
    }
    return len;
}

int esp8266_close(int fd)
{
    for (int i = 0; i < 2; i++) {
        if (atcmd_send(&at, "AT+CIPCLOSE=%d", fd) && atcmd_recv(&at, "OK\n")) {
            if (!socket_open[fd]) {
                // recv(processing OOBs) needs to be done first
                put_fd(fd);
                return 0;
            }
        }
    }
    return -1;
}
