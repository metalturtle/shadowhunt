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
// #include <box2d/box2d.h>
#include <chipmunk/chipmunk.h>
// #include <math.h>

EngineParameters engineParameters;
long beginGameTick;

SDL_FPoint mouseScreenPos;
// SDL_FRect cameraRect;
// SDL_Window *window;
// SDL_Renderer *renderer;
static byte isServer;

byte recvBuffer[MAX_MSGLEN];

// int SCREEN_WIDTH;
// int SCREEN_HEIGHT;

cvar_t *cv_isServer;
// b2WorldId worldId;
cpSpace *worldId;

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
    /* Opt-in deterministic input for the local multiplayer smoke harness. */
    static bool testKeyReported;
    const char *testKey = getenv("SHADOWHUNT_TEST_KEYS");
    if(!isServer && testKey != NULL && testKey[0] != '\0') {
        for(const char *key = testKey; *key != '\0'; key++) {
            if(strchr("wsadt", *key) != NULL)
                engineParameters.KEYPRESSED[(unsigned char)*key] = true;
        }
        if(!testKeyReported) {
            printf("test input active: %s\n", testKey);
            testKeyReported = true;
        }
    }

    const char *testMouse = getenv("SHADOWHUNT_TEST_MOUSE");
    if(!isServer && testMouse != NULL) {
        float x, y;
        if(sscanf(testMouse, "%f,%f", &x, &y) == 2)
            addSysEvent(SYSEVENT_MOUSE, (int)(x * 10000), (int)(y * 10000), NULL);
    }

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

    for(int i = 0; i < 256; i++) {
        if(engineParameters.KEYPRESSED[i] == true) {
            addSysEvent(SYSEVENT_KEY, i, qtrue, NULL);
        }
    }

    /* Drain every queued datagram (bounded) so a full server keeps up with
     * eight clients sending at 20 Hz each. */
    for(int packets = 0; packets < 96; packets++)
    {
        stream_init(&recvbs, recvBuffer, MAX_MSGLEN);
        if((ret = net_getPacket(&fromAddr, &recvbs)) <= 0)
            break;

        len = sizeof(netaddr_t) + ret;
        buf = (byte *) zidmalloc(TEMPORARYZONE, len);

        zmemcpy(buf, &fromAddr, sizeof(netaddr_t));
        zmemcpy(buf + sizeof(netaddr_t), recvBuffer, ret);
        addSysEvent(SYSEVENT_PACKET, len, 0, buf);
    }
}

void initEngineParameters(bool isServer) {
    engineParameters.aspectRatio = 1;
    cameraRect.x = 0;
    cameraRect.y = 0;
    cameraRect.w = 100;
    cameraRect.h = engineParameters.aspectRatio * cameraRect.w;
    engineParameters.windowWidth = 1280;
    engineParameters.windowHeight = 800;
    engineParameters.screenFPS = isServer? 60 : 60;
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
    /* Line-buffer logs so tail -f (play.sh) and crash logs stay current. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    // SCREEN_WIDTH = 600;
    // SCREEN_HEIGHT = 600;
    createThreeZones(1024*1024, 1024*1024*20, 1024*1024);

    cvar_init();

    int port = 8000;

    if(argc > 1)
    {
        cv_isServer = cvar_get("isServer", "0");
        isServer = 0;
        port = atoi(argv[1]);
        cvar_get("serverHost", argc > 2 ? argv[2] : "127.0.0.1");
        cvar_get("serverPort", argc > 3 ? argv[3] : "8000");
    } else 
    {
        cv_isServer = cvar_get("isServer", "1");
        isServer = 1;
    }

    if(com_verbose()) printf("isServer: %d %d\n", isServer, cvar_getInt("isServer"));

    initEngineParameters(isServer);

    int success;
    if(cv_isServer->intval) {
        /* SHADOWHUNT_SERVER_PORT lets tests run several servers at once. */
        const char *serverPort = getenv("SHADOWHUNT_SERVER_PORT");
        int listenPort = serverPort != NULL && atoi(serverPort) > 0 ? atoi(serverPort) : 8000;
        printf("server listening on UDP port %d\n", listenPort);
        success = net_init(listenPort);
    } else {
        success = net_init(port);
    }

    if(success < 0)
    {
        com_error(ERR_FATAL, "ERROR: failed to init net layer\n");
    }

    netcon_init();

    if(com_verbose()) printf("created world\n");
    // b2WorldDef worldDef = b2DefaultWorldDef();
    // worldDef.gravity = (b2Vec2){0.0f, 0.0f};  // Set gravity (pointing down)
    // worldId = b2CreateWorld(&worldDef);
    // b2World_EnableSleeping(worldId, true);
    // b2World_EnableWarmStarting(worldId, false);
    // b2World_EnableContinuous(worldId, false);

    // worldId = cpSpaceNew();
    // cpSpaceSetGravity(worldId, cpv(0, 0));  // No gravity for top-down
    // cpSpaceSetIterations(worldId, 10);
    // cpSpaceSetSleepTimeThreshold(worldId, INFINITY);
    // cpSpaceSetIdleSpeedThreshold(worldId, 0);
    // cpSpaceSetCollisionSlop(worldId, 0.0f);
    // // cpSpaceSetCollisionBias(worldId, cpfpow(1.0f - 0.1f, 60.0f));
    // cpSpaceSetCollisionBias(worldId, 0.5);
    // printf("checking value %f \n", cpfpow(1.0f - 0.1f, 60.0f));

    eng_init();

    eng_setup();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer(cv_isServer->intval ? "Shadowhunt Server" : "Shadowhunt", engineParameters.windowWidth, engineParameters.windowHeight, cv_isServer->intval ? 0 : SDL_WINDOW_RESIZABLE, &engineParameters.window, &engineParameters.renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(SDL_SetRenderVSync(engineParameters.renderer, 1) == false) {
        SDL_Log("Couldn't enable vsync");
        return SDL_APP_FAILURE;
    }
    
    if(!cv_isServer->intval) {

        initGraphicsHandleSDL(engineParameters.renderer, engineParameters.windowWidth, engineParameters.windowHeight, GENERALZONE, qtrue);
    }
    else {
        initGraphicsHandleSDL(engineParameters.renderer, engineParameters.windowWidth, engineParameters.windowHeight, GENERALZONE, qfalse);
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
        {
            int key = event->key.key;
            // SDL keycodes use ASCII values for letters, but may be uppercase
            // when Shift is held. Gameplay bindings use lowercase characters.
            if (key >= 'A' && key <= 'Z')
                key += 'a' - 'A';
            if (key >= 0 && key < (int)(sizeof(engineParameters.KEYPRESSED) /
                                        sizeof(engineParameters.KEYPRESSED[0])))
                engineParameters.KEYPRESSED[key] = true;
        }

        break;
        case SDL_EVENT_KEY_UP:
        {
            int key = event->key.key;
            if (key >= 'A' && key <= 'Z')
                key += 'a' - 'A';
            if (key >= 0 && key < (int)(sizeof(engineParameters.KEYPRESSED) /
                                        sizeof(engineParameters.KEYPRESSED[0]))) {
                engineParameters.KEYPRESSED[key] = false;
                addSysEvent(SYSEVENT_KEY, key, qfalse, NULL);
            }
        }
        break;
        case SDL_EVENT_MOUSE_MOTION:
        mouseScreenPos.x = event->motion.x;
        mouseScreenPos.y = event->motion.y;

        // vec_normalize(&mouseScreenPos);
        mouseScreenPos.x /= engineParameters.windowWidth;
        mouseScreenPos.y /= engineParameters.windowHeight;

        // printf("mouseScreenPos %f %f %f %f\n", mouseScreenPos.x, mouseScreenPos.y, event->motion.x, event->motion.y);

        int i_xpos = (int)(mouseScreenPos.x * 10000);
        int i_ypos = (int)(mouseScreenPos.y * 10000);
    
        // printf("mouse x,y: %f, %f \n", d_xpos, d_ypos);
        addSysEvent(SYSEVENT_MOUSE, i_xpos, i_ypos, NULL);

        break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(event->button.button == SDL_BUTTON_LEFT)
                engineParameters.KEYPRESSED['t'] = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(event->button.button == SDL_BUTTON_LEFT) {
                engineParameters.KEYPRESSED['t'] = false;
                addSysEvent(SYSEVENT_KEY, 't', qfalse, NULL);
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            if(event->window.data1 > 0 && event->window.data2 > 0) {
                engineParameters.windowWidth = event->window.data1;
                engineParameters.windowHeight = event->window.data2;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            memset(engineParameters.KEYPRESSED, 0, sizeof(engineParameters.KEYPRESSED));
            break;
    }
    
    return SDL_APP_CONTINUE;
}

float toFixedDecimals(float val, int dec) {
    int ival = val * pow(10, dec);
    return ival / pow(10, dec);
}

void engine_sleep() {
    const Uint64 nsPerFrame = 1000000000 / engineParameters.screenFPS;
    Uint64 end = SDL_GetTicksNS() - engineParameters.currentAbsoluteTick;
    engineParameters.absoluteDeltaTime = max((double) end / 1000000000.0, engineParameters.tickRate);

#ifndef __EMSCRIPTEN__
    /* The browser paces frames itself (requestAnimationFrame). */
    if(end < nsPerFrame) {
        Uint64 sleepTime = nsPerFrame - end;
        SDL_DelayNS(sleepTime);
    }
#endif
    Uint64 currentTick = SDL_GetTicksNS();
    if(!engineParameters.isPaused) {
        // printf("checking diff %llu \n", currentTick - beginGameTick);
        engineParameters.gameDeltaTime = engineParameters.absoluteDeltaTime * engineParameters.bulletTimeRate;
        engineParameters.gameDeltaTime = toFixedDecimals(engineParameters.gameDeltaTime, 6);
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

        /* SHADOWHUNT_HEADLESS skips drawing for bots and test clients: the
         * simulation, prediction and networking still run every frame. */
        static int headless = -1;
        if(headless < 0)
            headless = getenv("SHADOWHUNT_HEADLESS") != NULL;
        if(!headless)
            renderSDL();

        eng_afterRender();
    }
    else {
        scanSysEvents();
        eng_runFrame();
    }
    // runEngine();

    // sdl_render();
    // clearSpaceCache();
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
