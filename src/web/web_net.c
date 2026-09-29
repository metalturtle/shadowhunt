/* Browser network layer: the same API as unix_net.c, carried over one
 * WebSocket. Browsers cannot open UDP sockets, so each datagram becomes one
 * binary WebSocket message; tools/web_server.js relays them to the unchanged
 * UDP game server. The connection layer's sequencing and acks run on top
 * exactly as they do natively. */

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <stdio.h>
#include <string.h>
#include "../basic/basic.h"

#define WEB_QUEUE_SIZE 256

typedef struct {
    int length;
    byte data[MAX_MSGLEN];
} webPacket_t;

static EMSCRIPTEN_WEBSOCKET_T socketHandle;
static bool socketOpen;
static webPacket_t *queue;
static int queueHead, queueTail;
static netaddr_t peerAddress;
static bool havePeer;

/* Same datagram budget as the native layer; the connection layer fragments
 * anything larger. */
int fragLimit = 1000;

char *net_errorString()
{
    return "websocket error";
}

qbool netAddrCmp(netaddr_t a, netaddr_t b)
{
    return a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] &&
           a.ip[3] == b.ip[3] && a.port == b.port ? qtrue : qfalse;
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
    snprintf(s, sizeof(s), "%i.%i.%i.%i:%hu", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.port);
    return s;
}

void net_getNetAddr(netaddr_t *a)
{
    memset(a, 0, sizeof(*a));
}

/* ws(s)://<page host>/ws unless the page URL carries ?server=<url>. */
EM_JS(char *, web_serverUrl, (void), {
    var params = new URLSearchParams(window.location.search);
    var url = params.get('server');
    if (!url) {
        var scheme = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
        url = scheme + window.location.host + '/ws';
    }
    return stringToNewUTF8(url);
});

static EM_BOOL onOpen(int type, const EmscriptenWebSocketOpenEvent *event, void *user)
{
    socketOpen = true;
    printf("web net: connected\n");
    return EM_TRUE;
}

static EM_BOOL onClose(int type, const EmscriptenWebSocketCloseEvent *event, void *user)
{
    socketOpen = false;
    printf("web net: closed (%d)\n", event->code);
    return EM_TRUE;
}

static EM_BOOL onError(int type, const EmscriptenWebSocketErrorEvent *event, void *user)
{
    printf("web net: error\n");
    return EM_TRUE;
}

static EM_BOOL onMessage(int type, const EmscriptenWebSocketMessageEvent *event, void *user)
{
    if(event->isText || event->numBytes == 0 || event->numBytes > MAX_MSGLEN)
        return EM_TRUE;
    if(queueTail - queueHead >= WEB_QUEUE_SIZE) {
        printf("web net: receive queue full, dropping a packet\n");
        return EM_TRUE;
    }
    webPacket_t *packet = &queue[queueTail % WEB_QUEUE_SIZE];
    memcpy(packet->data, event->data, event->numBytes);
    packet->length = (int)event->numBytes;
    queueTail++;
    return EM_TRUE;
}

int net_init(int portNum)
{
    if(!emscripten_websocket_is_supported()) {
        com_printf("ERROR: this browser has no WebSocket support\n");
        return -1;
    }

    queue = (webPacket_t *)malloc(sizeof(webPacket_t) * WEB_QUEUE_SIZE);
    if(queue == NULL)
        return -1;

    char *url = web_serverUrl();
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = url;
    attributes.protocols = NULL;
    socketHandle = emscripten_websocket_new(&attributes);
    printf("web net: connecting to %s\n", url);
    free(url);
    if(socketHandle <= 0)
        return -1;

    emscripten_websocket_set_onopen_callback(socketHandle, NULL, onOpen);
    emscripten_websocket_set_onclose_callback(socketHandle, NULL, onClose);
    emscripten_websocket_set_onerror_callback(socketHandle, NULL, onError);
    emscripten_websocket_set_onmessage_callback(socketHandle, NULL, onMessage);
    return 0;
}

int net_sendPacket(netaddr_t *a, bitstream_t *msg)
{
    int length = MIN(msg->curbyte, fragLimit);
    /* Remember who we talk to: replies are reported as coming from there. */
    peerAddress = *a;
    havePeer = true;
    if(!socketOpen)
        return length;  /* the connection layer retries until the socket opens */
    if(emscripten_websocket_send_binary(socketHandle, msg->buf, length) != EMSCRIPTEN_RESULT_SUCCESS)
        return -1;
    return length;
}

int net_getPacket(netaddr_t *fromaddr, bitstream_t *msg)
{
    stream_init(msg, msg->buf, msg->bufsize);
    if(queueHead == queueTail || !havePeer)
        return -2;

    webPacket_t *packet = &queue[queueHead % WEB_QUEUE_SIZE];
    queueHead++;
    if(packet->length >= msg->bufsize)
        return -1;

    memcpy(msg->buf, packet->data, packet->length);
    msg->datalen = packet->length;
    *fromaddr = peerAddress;
    return packet->length;
}

qbool net_isReady(void)
{
    return socketOpen ? qtrue : qfalse;
}

void net_sleep(int msec)
{
    (void)msec;
}
