#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "clientsocket.h"

char *CreateAndCopyString(char *_dstString, const char *_srcString)
{
    if (_srcString == NULL)
    {
        if (_dstString != NULL)
            delete [] _dstString;

        return NULL;
    }

    if (_dstString != NULL)
    {
        if (strlen(_dstString) <= strlen(_srcString))
        {
            delete [] _dstString;
            _dstString = NULL;
        }
    }
    if (_dstString == NULL)
    {
        _dstString = new char[strlen(_srcString)+1];
        if (_dstString == NULL)
        {
            return NULL;
        }
    }
    strcpy(_dstString, _srcString);
    return _dstString;
}

int getIPAddress(const char *host, char *ip){
    //printf("C1\n");
    struct hostent *hh = gethostbyname(host);
    //printf("C2\n");
    struct in_addr *add = (in_addr *)hh->h_addr_list[0];
    //printf("C3\n");
    strcpy(ip, inet_ntoa(add[0]));
    //printf("C4\n");
    return 1;
}

struct ClientSocket *socket_create(char *ip, int port){
    struct ClientSocket *socket = (ClientSocket *)malloc(sizeof(struct ClientSocket));
    socket->ip = CreateAndCopyString(NULL, ip);
    socket->port = port;
    socket->getIp = getIPAddress;
    return(socket);
}

int socket_connect(struct ClientSocket *sock){
    struct sockaddr_in addr;
    int result;

    sock->fd = socket(AF_INET, SOCK_STREAM, 0);

    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_port = htons(sock->port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(sock->ip);

    result = connect(sock->fd, (struct sockaddr *)&addr, sizeof(addr));
    return(result);
}

int socket_send(struct ClientSocket *sock, char *data, int len){
    return send(sock->fd, data, len, 0);
}

int socket_receive(struct ClientSocket *sock, char *data, int len){
    int total = 0;
    int ret = 0;
    do
    {
        ret = recv(sock->fd, (void *)&data[total], len-total, 0);
        if (ret > 0)
            total += ret;
    }
    while (ret > 0);

    if (ret < 0)
        return ret;

    data[total < len ? total : len-1] = 0;
    return total;
}

int socket_close(struct ClientSocket *sock){
    return close(sock->fd);
}

void socket_destroy(struct ClientSocket *sock){
    if (sock->fd != 0)
        socket_close(sock);
    CreateAndCopyString(sock->ip, NULL);
    free(sock);
}

