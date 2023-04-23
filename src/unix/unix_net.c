#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "../basic/basic.h"

struct sockaddr_in sockAddr;
struct sockaddr_in sendAddr;

int sockfd;

int fragLimit = 1000;

char *net_errorString()
{
    int code;
    code = errno;
    return strerror(code);
}

qbool netAddrCmp(netaddr_t a, netaddr_t b)
{
    if((a.ip[0] == b.ip[0]) 
    && (a.ip[1] == b.ip[1]) 
    && (a.ip[2] == b.ip[2]) 
    && (a.ip[3] == b.ip[3])
    && (a.port == b.port))
    {
        return qtrue;
    }
    return qfalse;
}

void netAddrSet(netaddr_t *a, int ip1, int ip2, int ip3, int ip4, int port)
{
    a->ip[0] = ip1;
    a->ip[1] = ip2;
    a->ip[2] = ip3;
    a->ip[3] = ip4;
    a->port = port;
}

const char *netAddrToString(netaddr_t a)
{
    static char s[64];
    sprintf(s, "%i.%i.%i.%i:%hu", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.port);
    return s;
}

void netAddrToSockAddr(netaddr_t *a, struct sockaddr_in *s)
{
	s->sin_family = AF_INET;
	*(int *)&s->sin_addr = *(int *)&a->ip;
	s->sin_port = htons(a->port);
}

void sockAddrToNetAddr(netaddr_t *a, struct sockaddr_in *s)
{
    *(int *)&a->ip = *(int *)&s->sin_addr;
    a->port = ntohs(s->sin_port);
}

void net_getNetAddr(netaddr_t * a)
{
    sockAddrToNetAddr(a, &sockAddr);
}

int net_init(int portNum)
{
    qbool qb;
    int newsock;
    char portStr[32];
    sprintf(portStr, "%d", portNum); 
    cvar_t *cv_port = cvar_get("net_port", portStr);
    netaddr_t addr;
    if((newsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        com_printf("ERROR: failed to open udp socket %s", net_errorString());
        return -1;
    }

    qb = qtrue;
    if(ioctl(newsock, FIONBIO, &qb) == -1)
    {
        com_printf("ERROR: net_init: ioctl FIONBIO %s\n", net_errorString());
        return -1;
    }

    sockAddr.sin_addr.s_addr = INADDR_ANY;
    sockAddr.sin_port = htons(cv_port->intval);
    sockAddr.sin_family = AF_INET;
    if(bind(newsock, (const struct sockaddr*) &sockAddr, sizeof(sockAddr)) < 0)
    {
        close(newsock);
        return -1;
    }
    sockfd = newsock;

    sockAddrToNetAddr(&addr, &sockAddr);
    return 0;
}

int net_sendPacket(netaddr_t *a, bitstream_t *msg)
{
    netAddrToSockAddr(a, &sendAddr);
    int ret = sendto(sockfd, msg->buf, MIN(msg->curbyte, fragLimit), 0, (struct sockaddr*) &sendAddr, sizeof(struct sockaddr));
    return ret;
}

int net_getPacket(netaddr_t *fromaddr, bitstream_t *msg)
{
    int err;
    struct sockaddr_in from;
    unsigned int fromlen;
    fromlen = sizeof(from);
    stream_init(msg, msg->buf, msg->bufsize);
    int ret = recvfrom(sockfd, msg->buf, msg->bufsize, 0, (struct sockaddr *)&from, &fromlen);
    sockAddrToNetAddr(fromaddr, &from);

    if(ret == -1)
    {
        err = errno;
        if(err == EWOULDBLOCK || err == ECONNREFUSED)
        return -2;
        com_printf("Error in net_getPacket: %s from %s\n", net_errorString(), netAddrToString(*fromaddr));
        return -1;
    }
    if(ret == msg->bufsize)
    {
        com_printf("oversized packet from %s, retval=%d\n", netAddrToString(*fromaddr), ret);
        return -1;
    }

    msg->datalen = ret;

    return ret;
}

void net_sleep(int msec)
{
    struct timeval timeout;
    fd_set fdset;
    // extern qbool stdin_active;
    FD_ZERO(&fdset);
    // if(stdin_active)
    FD_SET(0, &fdset);
    FD_SET(sockfd, &fdset);
    timeout.tv_sec = msec/1000;
    timeout.tv_usec = (msec%1000)*1000;
    select(sockfd, &fdset, NULL, NULL, &timeout);
}