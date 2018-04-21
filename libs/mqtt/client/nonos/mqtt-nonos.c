#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/time.h>

#include "socket.h"

#include "mqtt-nonos.h"

void TimerInit(Timer* timer)
{
    timer->end_time = (struct timeval){0, 0};
}

char TimerIsExpired(Timer* timer)
{
    struct timeval now, res;
    gettimeofday(&now, NULL);
    timersub(&timer->end_time, &now, &res);
    return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}

void TimerCountdownMS(Timer* timer, unsigned int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timeval interval = {timeout / 1000, (timeout % 1000) * 1000};
    timeradd(&now, &interval, &timer->end_time);
}

void TimerCountdown(Timer* timer, unsigned int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timeval interval = {timeout, 0};
    timeradd(&now, &interval, &timer->end_time);
}

int TimerLeftMS(Timer* timer)
{
    struct timeval now, res;
    gettimeofday(&now, NULL);
    timersub(&timer->end_time, &now, &res);
    //printf("left %d ms\n",
    //       (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000);
    return (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000;
}

static int _read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    Timer timer;
    TimerInit(&timer);
    TimerCountdownMS(&timer, timeout_ms);

    int bytes = 0;
    while (bytes < len && !TimerIsExpired(&timer)) {
        int rc = socket_recv(n->my_socket, &buffer[bytes], (size_t)(len - bytes));
        if (rc == -1) {
            break;
        } else if (rc == 0) {
            bytes = 0;
            break;
        } else {
            bytes += rc;
        }
    }
    return bytes;
}


static int _write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    Timer timer;
    TimerInit(&timer);
    TimerCountdownMS(&timer, timeout_ms);

    int bytes = 0;
    while (bytes < len && !TimerIsExpired(&timer)) {
        int	rc = socket_send(n->my_socket, &buffer[bytes], len - bytes);
        if (rc < 0) {
            bytes = rc;
            break;
        } else if (rc == 0) {
            bytes = 0;
            break;
        } else {
            bytes += rc;
        }
    }
    return bytes;
}

void NetworkInit(Network* n)
{
    n->my_socket = 0;
    n->mqttread = _read;
    n->mqttwrite = _write;
}


int NetworkConnect(Network* n, char* addr, int port)
{
    int rc = -1;

    n->my_socket = socket_connect(addr, port);
    if (n->my_socket != -1) {
        rc = 0;
    }

    return rc;
}


void NetworkDisconnect(Network* n)
{
    close(n->my_socket);
}

