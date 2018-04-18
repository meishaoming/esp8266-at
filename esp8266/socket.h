#ifndef _SOCK_H_
#define _SOCK_H_

typedef struct socket_interface {
    int (*init)(const char *ssid, const char *password);
    int (*connect)(const char *address, const unsigned port);
    int (*close)(int sockfd);
    int (*send)(int sockfd, const void *buf, unsigned int nbytes);
    int (*recv)(int sockfd, void *buf, unsigned int nbytes);
} socket_interface;

int socket_init(socket_interface *si, const char *ssid, const char *password);

int socket_connect(const char *address, const unsigned port);
int socket_close(int sockfd);
int socket_send(int sockfd, const void *buf, unsigned int nbytes);
int socket_recv(int sockfd, void *buf, unsigned int nbytes);

#endif /* #ifndef _SOCK_H_ */
