#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include "esp8266.h"
#include "socket_esp8266.h"

#include "../common/config.h"

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

    struct timeval _tv1;
    gettimeofday(&_tv1, NULL);

    int rc = socket_init(&socket_esp8266, SSID, PASS);
    if (rc) {
        printf("socket init failed %d\n", rc);
        exit(-1);
    }

    struct timeval _tv2;
    gettimeofday(&_tv2, NULL);
    printf("socket_init comsume %ldms\n",
           (_tv2.tv_sec - _tv1.tv_sec)*1000 + (_tv2.tv_usec - _tv1.tv_usec)/1000);
}

static bool connected = false;
static int sockfd;


static int net_process(void)
{
    struct timeval _tv1;
    gettimeofday(&_tv1, NULL);
    static unsigned long count = 0;
    char send_buf[16];

    int n = snprintf(send_buf, sizeof(send_buf), "%lu", count);
    int rc = socket_send(sockfd, send_buf, n);
    if (rc < 0) {
        if (rc == NSAPI_ERROR_CONNECTION_LOST) {
            connected = false;
        }
        printf("socket send failed\n");
        return rc;
    }

    count++;

    struct timeval _tv2;
    gettimeofday(&_tv2, NULL);
    printf("socket_send comsume %ldms\n",
           (_tv2.tv_sec - _tv1.tv_sec)*1000 + (_tv2.tv_usec - _tv1.tv_usec)/1000);

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

    struct timeval _tv3;
    gettimeofday(&_tv3, NULL);
    printf("socket recv comsume %ldms\n",
           (_tv3.tv_sec - _tv2.tv_sec)*1000 + (_tv3.tv_usec - _tv2.tv_usec)/1000);

    return 0;
}

int main(void)
{
    init();

    while (true) {

        if (!connected) {

            struct timeval _tv1;
            gettimeofday(&_tv1, NULL);

            sockfd = socket_connect(ADDR, PORT);
            if (sockfd >= 0) {
                connected = true;
            } else {
                printf("socket connect failed: %d\n", sockfd);
            }

            struct timeval _tv2;
            gettimeofday(&_tv2, NULL);
            printf("socket_connect comsume %ldms\n",
                   (_tv2.tv_sec - _tv1.tv_sec)*1000 + (_tv2.tv_usec - _tv1.tv_usec)/1000);

        } else {
            net_process();
        }
        printf("------------------------------------------\n\n\n\n\n\n");
    }
}
