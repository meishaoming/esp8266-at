#include <stdio.h>
#include <string.h>
#include "esp8266.h"
#include "socket_esp8266.h"

static int _started = 0;

static const char *_get_ipaddress(void)
{
    if (!_started) {
        return NULL;
    }
    const char *ip_buff = esp8266_get_ipaddress();

    if (!ip_buff || strcmp(ip_buff, "0.0.0.0") == 0) {
        return NULL;
    }
    return ip_buff;
}

static nsapi_error_t _startup(const int8_t wifi_mode)
{
    if (!_started) {
        if (!esp8266_startup(wifi_mode)) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
    }
    return NSAPI_ERROR_OK;
}

static int _init(void)
{
    if (!esp8266_reset()) {
        return -1;
    }

    int version = esp8266_get_firmware_version();
    printf("version : %d\n", version);

    int mode = esp8266_get_default_wifi_mode();
    printf("mode %d\n", mode);
    if (mode < 0) {
        return -1;
    }

    if (mode != ESP8266_WIFIMODE_STATION) {
        if (!esp8266_set_default_wifi_mode(ESP8266_WIFIMODE_STATION)) {
            return -1;
        }
    }
    return 0;
}

int esp_init(const char *ssid, const char *pass)
{
    nsapi_error_t status;

    if (strlen(ssid) == 0) {
        return NSAPI_ERROR_NO_SSID;
    }

    status = _init();
    if (status != NSAPI_ERROR_OK) {
        return status;
    }

    if (_get_ipaddress()) {
        return NSAPI_ERROR_IS_CONNECTED;
    }

    status = _startup(ESP8266_WIFIMODE_STATION);
    if (status != NSAPI_ERROR_OK) {
        return status;
    }
    _started = 1;

    if (!esp8266_dhcp(1, 1)) {
        return NSAPI_ERROR_DHCP_FAILURE;
    }

    int connect_error = esp8266_connect(ssid, pass);
    if (connect_error) {
        return connect_error;
    }

    if (!_get_ipaddress()) {
        return NSAPI_ERROR_DHCP_FAILURE;
    }

    return NSAPI_ERROR_OK;
}

static int esp_connect(const char *address, const unsigned port)
{
    return esp8266_open_tcp(address, port, 0);
}

static int esp_close(int sockfd)
{
    return esp8266_close(sockfd);
}

static int esp_send(int sockfd, const void *buf, unsigned int nbytes)
{
    return esp8266_send_tcp(sockfd, buf, nbytes);
}

static int esp_recv(int sockfd, void *buf, unsigned int nbytes)
{
    return esp8266_recv_tcp(sockfd, buf, nbytes);
}

socket_interface socket_esp8266 = {
    .init = esp_init,
    .connect = esp_connect,
    .close = esp_close,
    .send = esp_send,
    .recv = esp_recv,
};
