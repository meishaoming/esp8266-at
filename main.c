#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <unistd.h>
#include "esp8266.h"
#include "socket_esp8266.h"

#include "config.h"

extern int serial_open_nonblock(const char *devpath);

static const char *portname = "/dev/cu.SLAB_USBtoUART";

static int serial_fd = -1;

static int linux_putc(unsigned char ch, int timeout)
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(serial_fd, &wfds);

    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000,
    };

    int count = select(serial_fd + 1, NULL, &wfds, NULL, &tv);
    if (count > 0) {
        return write(serial_fd, &ch, 1) == 1 ? 0 : -1;
    } else {
        printf("putc, select failed\n");
        return -1;
    }
}

static int linux_getc(int timeout)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(serial_fd, &rfds);

    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000,
    };
    int count = select(serial_fd + 1, &rfds, NULL, NULL, &tv);
    if (count > 0)
    {
        unsigned char ch;
        return read(serial_fd, &ch, 1) == 1 ? ch : -1;
    }
    else
    {
        return -1;
    }
}


static atcmd_ops ops = {
    .getc_timeout = linux_getc,
    .putc_timeout = linux_putc,
};

static void init()
{
    serial_fd = serial_open_nonblock(portname);
    if (serial_fd < 0) {
        printf("open %s failed\n", portname);
        exit(-1);
    }

    esp8266_init(&ops);

    int rc = socket_init(&socket_esp8266, SSID, PASS);
    if (rc) {
        printf("socket init failed %d\n", rc);
        exit(-1);
    }
}

static bool connected = false;
static int sockfd;

static int net_process(void)
{
    int rc = socket_send(sockfd, "hello", 5);
    if (rc < 0) {
        if (rc == NSAPI_ERROR_CONNECTION_LOST) {
            connected = false;
            return rc;
        }
        else {
            printf("socket send failed\n");
            exit(-1);
        }
    }

    char buf[128];
    rc = socket_recv(sockfd, buf, sizeof(buf));
    if (rc == NSAPI_ERROR_CONNECTION_LOST) {
        connected = false;
        return rc;
    }
    if (rc < 0) {
        printf("socket recv failed\n");
        return rc;
    }
    printf("received %d bytes: %.*s\n", rc, rc, buf);

    return 0;
}

int main(void)
{
    init();

    while (true) {

        if (!connected) {
            sockfd = socket_connect(ADDR, PORT);
            if (sockfd >= 0) {
                connected = true;
            } else {
                printf("socket connect failed: %d\n", sockfd);
            }
        } else {
            net_process();
        }
        printf("------------------------------------------\n\n\n\n\n\n");
    }
}
