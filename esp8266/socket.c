#include "socket.h"

static socket_interface *sock = 0;

int socket_init(socket_interface *si, const char *ssid, const char *password)
{
    sock = si;
    return sock ? sock->init(ssid, password) : -1;
}

int socket_connect(const char *address, const unsigned port)
{
    return sock ? sock->connect(address, port) : -1;
}

int socket_close(int sockfd)
{
    return sock ? sock->close(sockfd) : -1;
}

int socket_send(int sockfd, const void *buf, unsigned int nbytes)
{
    return sock ? sock->send(sockfd, buf, nbytes) : -1;
}

int socket_recv(int sockfd, void *buf, unsigned int nbytes)
{
    return sock ? sock->recv(sockfd, buf, nbytes) : -1;
}
