#ifndef _ESP8266_H_
#define _ESP8266_H_

#include <stdbool.h>
#include "socket.h"
#include "atcmd.h"

enum nsapi_error
{
    NSAPI_ERROR_OK                          = 0,        /*!< no error */
    NSAPI_ERROR_WOULD_BLOCK                 = -3001,    /*!< no data is not available but call is non-blocking */
    NSAPI_ERROR_UNSUPPORTED                 = -3002,    /*!< unsupported functionality */
    NSAPI_ERROR_PARAMETER                   = -3003,    /*!< invalid configuration */
    NSAPI_ERROR_NO_CONNECTION               = -3004,    /*!< not connected to a network */
    NSAPI_ERROR_NO_SOCKET                   = -3005,    /*!< socket not available for use */
    NSAPI_ERROR_NO_ADDRESS                  = -3006,    /*!< IP address is not known */
    NSAPI_ERROR_NO_MEMORY                   = -3007,    /*!< memory resource not available */
    NSAPI_ERROR_NO_SSID                     = -3008,    /*!< ssid not found */
    NSAPI_ERROR_DNS_FAILURE                 = -3009,    /*!< DNS failed to complete successfully */
    NSAPI_ERROR_DHCP_FAILURE                = -3010,    /*!< DHCP failed to complete successfully */
    NSAPI_ERROR_AUTH_FAILURE                = -3011,    /*!< connection to access point failed */
    NSAPI_ERROR_DEVICE_ERROR                = -3012,    /*!< failure interfacing with the network processor */
    NSAPI_ERROR_IN_PROGRESS                 = -3013,    /*!< operation (eg connect) in progress */
    NSAPI_ERROR_ALREADY                     = -3014,    /*!< operation (eg connect) already in progress */
    NSAPI_ERROR_IS_CONNECTED                = -3015,    /*!< socket is already connected */
    NSAPI_ERROR_CONNECTION_LOST             = -3016,    /*!< connection lost */
    NSAPI_ERROR_CONNECTION_TIMEOUT          = -3017,    /*!< connection timed out */
    NSAPI_ERROR_ADDRESS_IN_USE              = -3018,    /*!< Address already in use */
};

typedef signed int nsapi_error_t;

#define ESP8266_WIFIMODE_STATION            1
#define ESP8266_WIFIMODE_SOFTAP             2
#define ESP8266_WIFIMODE_STATION_SOFTAP     3

void esp8266_init(atcmd_ops *ops);
bool esp8266_reset(void);
int esp8266_startup(int mode);
bool esp8266_dhcp(bool enabled, int mode);

int esp8266_get_firmware_version(void);
int esp8266_get_default_wifi_mode(void);
bool esp8266_set_default_wifi_mode(const int8_t mode);
const char *esp8266_get_ipaddress(void);
const char *esp8266_get_macaddress(void);
const char *esp8266_get_gateway(void);
const char *esp8266_get_netmask(void);
int esp8266_get_rssi(void);

int esp8266_connect(const char *ap, const char *password);
int esp8266_open_tcp(const char *addr, int port, int keepalive);
int esp8266_send_tcp(int fd, const void *data, unsigned int amount);
int esp8266_recv_tcp(int fd, void *data, unsigned int amount);
int esp8266_close(int fd);

#endif
