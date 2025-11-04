#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <stdarg.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "../basic/cJSON.h"
#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "../engine/engine.h"
#include "../render/render.h"
#include "../render/sdl_render.h"
// #include <math.h>

EngineParameters engineParameters;
long beginGameTick;

SDL_FPoint mouseScreenPos;
// SDL_FRect cameraRect;
// SDL_Window *window;
// SDL_Renderer *renderer;
static byte isServer;

byte recvBuffer[MAX_MSGLEN];

int SCREEN_WIDTH;
int SCREEN_HEIGHT;

cvar_t *cv_isServer;

/********************EVENTS********************/


#define MAXEVENTLIMIT 256
sysEvent_t sysEventQueue[MAXEVENTLIMIT];
static int evHead = 0, evTail = 0;

double min(double a, double b) {
    return a < b ? a : b;
}

double max(double a, double b) {
    return a > b ? a : b;
}


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

// void addKeyEvents()
// {
//     for(int i = 0; i < 256; i++)
//     {
//         if(glfwGetKey(window, i) == GLFW_PRESS)
//         {
//             addSysEvent(SYSEVENT_KEY, i, qtrue, NULL);
//         }
//     }
// }

// void addMouseEvents()
// {
//     double d_xpos, d_ypos;
//     glfwGetCursorPos(window, &d_xpos, &d_ypos);


//     d_xpos = MAX(d_xpos, 0);
//     d_ypos = MAX(d_ypos, 0);

//     d_ypos = SCREEN_HEIGHT - d_ypos;

//     d_xpos = MIN(d_xpos, SCREEN_WIDTH);
//     d_ypos = MIN(d_ypos, SCREEN_HEIGHT);

//     d_xpos = d_xpos/SCREEN_WIDTH;
//     d_ypos = d_ypos/SCREEN_HEIGHT;

//     int i_xpos = (int)(d_xpos * 10000);
//     int i_ypos = (int)(d_ypos * 10000);

//     // printf("mouse x,y: %f, %f \n", d_xpos, d_ypos);
//     addSysEvent(SYSEVENT_MOUSE, i_xpos, i_ypos, NULL);
// }

// void fb_size_callback(GLFWwindow *window, int width, int height)
// {
//     glViewport(0, 0, width, height);
//     printf("view changed \n");
// }

// GLFWwindow *initWindow(int swidth, int sheight)
// {
//     glfwInit();
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//     GLFWwindow *window = glfwCreateWindow(swidth, sheight, "project", NULL, NULL);
//     if (window == NULL)
//     {
//         printf("failed to create GLFW window\n");
//         return NULL;
//     }
//     glfwMakeContextCurrent(window);

//     if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
//     {
//         printf("Failed to init GLAD\n");
//         return NULL;
//     }

//     glfwSetFramebufferSizeCallback(window, fb_size_callback);

//     return window;
// }

void scanSysEvents()
{
    netaddr_t fromAddr;
    bitstream_t recvbs;
    int ret = 0;
    byte *buf;
    int len;

    // if(!isServer)
    // {
    //     addMouseEvents();
    //     addKeyEvents();
    // }


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

void initEngineParameters() {
    engineParameters.aspectRatio = 1;
    cameraRect.x = 0;
    cameraRect.y = 0;
    cameraRect.w = 100;
    cameraRect.h = engineParameters.aspectRatio * cameraRect.w;
    engineParameters.windowWidth = 800;
    engineParameters.windowHeight = engineParameters.aspectRatio * engineParameters.windowWidth;
    engineParameters.screenFPS = 60;
    engineParameters.tickRate = 1.0/engineParameters.screenFPS;
    engineParameters.gameDeltaTime = 0;
    engineParameters.absoluteDeltaTime =0;
    engineParameters.currentAbsoluteTick = 0;
    engineParameters.currentGameTick = 0;
    engineParameters.isPaused = false;
    engineParameters.bulletTimeRate = 1;
    // engineParameters.KEYPRESSED = {0};
    for(int i = 0; i < 256; i++) {
        engineParameters.KEYPRESSED[i] = false;
    }
    engineParameters.toWindowRatioX = engineParameters.windowWidth/ cameraRect.w;
    engineParameters.toWindowRatioY = engineParameters.windowHeight/ cameraRect.h;
    engineParameters.toWorldRatioX = cameraRect.w/ engineParameters.windowWidth;
    engineParameters.toWorldRatioY = cameraRect.h/ engineParameters.windowHeight;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SCREEN_WIDTH = 600;
    SCREEN_HEIGHT = 600;
    createThreeZones(1024*1024, 1024*1024*20, 1024*1024);

    cvar_init();

    initEngineParameters();

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

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer(cv_isServer->intval ? "Server" : "Client", SCREEN_WIDTH, SCREEN_HEIGHT, 0, &engineParameters.window, &engineParameters.renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(SDL_SetRenderVSync(engineParameters.renderer, 1) == false) {
        SDL_Log("Couldn't enable vsync");
        return SDL_APP_FAILURE;
    }
    
    if(!cv_isServer->intval) {

        initGraphicsHandleSDL(engineParameters.renderer, SCREEN_WIDTH, SCREEN_HEIGHT, GENERALZONE, qtrue);
        closeLevelFile();
    }
    else {
        initGraphicsHandleSDL(engineParameters.renderer, SCREEN_WIDTH, SCREEN_HEIGHT, GENERALZONE, qfalse);
        closeLevelFile();
    }

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{

    // mouseWorldPos = convertPointToWorldCoord(mouseScreenPos);
    switch(event->type) {
        case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
        break;
        case SDL_EVENT_KEY_DOWN:
        // printf("adding sys event %d \n", event->key.key);
        // engineParameters.KEYPRESSED[event->key.key] = true;
        addSysEvent(SYSEVENT_KEY, event->key.key, qtrue, NULL);
        break;
        // case SDL_EVENT_KEY_UP:
        // engineParameters.KEYPRESSED[event->key.key] = false;
        // break;
        case SDL_EVENT_MOUSE_MOTION:
        mouseScreenPos.x = event->motion.x;
        mouseScreenPos.y = event->motion.y;

        // vec_normalize(&mouseScreenPos);
        mouseScreenPos.x /= engineParameters.windowWidth;
        mouseScreenPos.y /= engineParameters.windowHeight;

        int i_xpos = (int)(mouseScreenPos.x * 10000);
        int i_ypos = (int)(mouseScreenPos.y * 10000);
    
        // printf("mouse x,y: %f, %f \n", d_xpos, d_ypos);
        addSysEvent(SYSEVENT_MOUSE, i_xpos, i_ypos, NULL);

        break;
    }
    
    return SDL_APP_CONTINUE;
}

// 212501583
// 181118625
void engine_sleep() {
    const Uint64 nsPerFrame = 1000000000 / engineParameters.screenFPS;
    Uint64 end = SDL_GetTicksNS() - engineParameters.currentAbsoluteTick;
    engineParameters.absoluteDeltaTime = max((double) end / 1000000000.0, engineParameters.tickRate);

    if(end < nsPerFrame) {
        Uint64 sleepTime = nsPerFrame - end;
        SDL_DelayNS(sleepTime);
    }
    Uint64 currentTick = SDL_GetTicksNS();
    if(!engineParameters.isPaused) {
        // printf("checking diff %llu \n", currentTick - beginGameTick);
        engineParameters.gameDeltaTime = engineParameters.absoluteDeltaTime * engineParameters.bulletTimeRate;
        Uint64 timePassed = currentTick - beginGameTick;
        timePassed *= engineParameters.bulletTimeRate;
        engineParameters.currentGameTick += (timePassed);
    }
    
    engineParameters.currentAbsoluteTick = currentTick;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    beginGameTick = SDL_GetTicksNS();
    SDL_RenderClear(engineParameters.renderer);


    SDL_SetRenderDrawColor(engineParameters.renderer, 0, 0, 0, 255);

    if(!cv_isServer->intval) {
        scanSysEvents();

        eng_runFrame();

        eng_afterRender();

        renderSDL();
    }
    else {
        scanSysEvents();
        eng_runFrame();
    }
    // runEngine();

    // sdl_render();

    engine_sleep();


    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    // SDL_DestroyTexture(texture);
    // b2DestroyWorld(worldId);
}


// int main(int argc, char **argv)
// {
//     SCREEN_WIDTH = 600;
//     SCREEN_HEIGHT = 600;
//     createThreeZones(1024*1024, 1024*1024*20, 1024*1024);

//     cvar_init();

//     int port;

//     if(argc > 1)
//     {
//         cv_isServer = cvar_get("isServer", "0");
//         isServer = 0;
//         port = atoi(argv[1]);
//     } else 
//     {
//         cv_isServer = cvar_get("isServer", "1");
//         isServer = 1;
//     }

//     int success;
//     if(cv_isServer->intval) {
//         success = net_init(8000);
//     } else {
//         success = net_init(port);
//     }

//     if(success < 0)
//     {
//         com_error(ERR_FATAL, "ERROR: failed to init net layer\n");
//     }

//     netcon_init();

//     openLevelFile();

//     eng_init();

//     if(!cv_isServer->intval) {
//         window = initWindow(SCREEN_WIDTH, SCREEN_HEIGHT);
//         initGraphicsHandle(SCREEN_WIDTH, SCREEN_HEIGHT, GENERALZONE, qtrue);
//         closeLevelFile();

//         while(!glfwWindowShouldClose(window))
//         {
//             scanSysEvents();
//             eng_runFrame();
//             render();
//             glfwSwapBuffers(window);
//             glfwPollEvents();
//             eng_afterRender();
//         }
//     }
//     else {
//         initGraphicsHandle(SCREEN_WIDTH, SCREEN_HEIGHT, GENERALZONE, qfalse);
//         closeLevelFile();

//         while(qtrue)
//         {
//             scanSysEvents();
//             eng_runFrame();
//         }
//     }

//     return 0;
// }