/*
 * File:   ClientSocket.h
 * Author: Practical Code Solutions
 *
 * Created on January 31, 2012, 9:56 AM
 */

#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
struct ClientSocket {
    int fd;
    char *ip;
    int port;
    int (*getIp)(const char *, char *);
};
#ifdef __cplusplus
};
#endif

int getIPAddress(const char *host, char *ip);
struct ClientSocket *socket_create(char *ip, int port);
int socket_connect(struct ClientSocket *sock);
int socket_send(struct ClientSocket *sock, char *data, int len);
int socket_receive(struct ClientSocket *sock, char *data, int len);
int socket_close(struct ClientSocket *sock);
void socket_destroy(struct ClientSocket *sock);
#endif /* CLIENTSOCKET_H */
