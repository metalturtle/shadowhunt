
#include <stdarg.h>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "../lib/cJSON/cJSON.c"
#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "../engine/engine.h"
#include "../render/render.h"



GLFWwindow *window;
static byte isServer;

char recvBuffer[MAX_MSGLEN];

/********************EVENTS********************/

#define MAXEVENTLIMIT 256
sysEvent_t sysEventQueue[MAXEVENTLIMIT];
static int evHead = 0, evTail = 0;


qbool isSysEventEmpty()
{
    if(evHead == evTail)
        return qtrue;
    return qfalse;
}

void addSysEvent(sysEventType_e type, int value, int value2, void *ptr)
{
    evTail++;
    sysEvent_t *event;
    if(evTail - evHead == MAXEVENTLIMIT)
    {
        printf("Event overflow\n");
        evHead++;
    }
    
    event = &sysEventQueue[evTail & (MAXEVENTLIMIT-1)];
    event->type = type;
    event->value = value;
    event->value2 = value2;
    event->ptr = ptr;
}

sysEvent_t *getSysEvent()
{
    sysEvent_t *ev;
    if(isSysEventEmpty())
    {
        return NULL;
    }
    evHead++;
    ev = &sysEventQueue[evHead & (MAXEVENTLIMIT-1)];

    return ev;
}

/********************GLFW********************/

void addKeyEvents()
{
    for(int i = 0; i < 256; i++)
    {
        if(glfwGetKey(window, i) == GLFW_PRESS)
        {
            addSysEvent(SYSEVENT_KEY, i, qtrue, NULL);
        }
    }
}

void fb_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
    printf("view changed \n");
}

GLFWwindow *initWindow(int swidth, int sheight)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow *window = glfwCreateWindow(swidth, sheight, "project", NULL, NULL);
    if (window == NULL)
    {
        printf("failed to create GLFW window\n");
        return NULL;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("Failed to init GLAD\n");
        return NULL;
    }

    glfwSetFramebufferSizeCallback(window, fb_size_callback);

    return window;
}

void scanSysEvents()
{
    netaddr_t fromAddr;
    bitstream_t recvbs;
    int ret = 0;
    byte *buf;
    int len;

    if(!isServer) addKeyEvents();


    stream_init(&recvbs, recvBuffer, MAX_MSGLEN);
    if((ret = net_getPacket(&fromAddr, &recvbs)) > 0)
    {

        len = sizeof(netaddr_t) + ret;
        buf = (byte *) zidmalloc(TEMPORARYZONE, len);

        // printf("checking buf=%p len=%d\n", buf, len);
        zmemcpy(buf, &fromAddr, sizeof(netaddr_t));
        zmemcpy(buf + sizeof(netaddr_t), recvBuffer, ret);
        addSysEvent(SYSEVENT_PACKET, len, 0, buf);
    }
}

cvar_t *cv_isServer;
int main(int argc, char **argv)
{
    int swidth = 600, sheight = 600;
    createThreeZones(1024*1024, 1024*1024*20, 1024*1024);

    cvar_init();

    int port;

    if(argc > 1)
    {
        cv_isServer = cvar_get("isServer", "0");
        isServer = 0;
        port = atoi(argv[1]);
    } else 
    {
        cv_isServer = cvar_get("isServer", "1");
        isServer = 1;
    }

    int success;
    if(cv_isServer->intval) {
        success = net_init(8000);
    } else {
        success = net_init(port);
    }

    if(success < 0)
    {
        com_error(ERR_FATAL, "ERROR: failed to init net layer\n");
    }

    netcon_init();

    openLevelFile();

    eng_init();

    if(!cv_isServer->intval) {
        window = initWindow(swidth, sheight);
        initGraphicsHandle(swidth, sheight, GENERALZONE, qtrue);
        closeLevelFile();

        while(!glfwWindowShouldClose(window))
        {
            scanSysEvents();
            eng_runFrame();
            render();
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    else {
        initGraphicsHandle(swidth, sheight, GENERALZONE, qfalse);
        closeLevelFile();

        while(qtrue)
        {
            scanSysEvents();
            eng_runFrame();
        }
    }

    return 0;
}