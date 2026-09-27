/**
 * SDL Render Module - Port from OpenGL to SDL3
 * 
 * This file provides SDL-based rendering functionality converted from OpenGL/GLAD.
 * It handles texture loading, sprite rendering, animated sprites, camera transformations,
 * and framebuffer/render target management for offscreen rendering.
 * 
 * Key Features:
 * - SDL_Texture-based texture management (replaces OpenGL textures)
 * - SDL_Renderer target switching for framebuffers (replaces OpenGL FBOs)
 * - Software-based sprite transformations (replaces OpenGL matrix transformations)
 * - Animated sprite support using texture atlases (replaces OpenGL texture arrays)
 * - Font rendering system
 * - Camera/viewport system with world-to-screen coordinate conversion
 */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <math.h>
#include "../basic/world_def.h"
#include "../basic/cJSON.h"
#include "render.h"
#include "../movement/movement.h"
#include "../engine/entity.h"
#include "../engine/engine.h"
#include "../engine/stealth.h"

/********************EXTERN DECLARATIONS********************/

// extern camera_t worldCamera;
SDL_FRect cameraRect;
extern world_t world;
extern entitySpriteList_t entSpriteList;
extern animatedSpriteList_t animSpriteList;
extern renderRayList_t renderRayList;

SDL_Texture *testTexture;

/********************SDL RENDER CONTEXT********************/

// Global SDL renderer (should be passed from main SDL initialization)
static SDL_Renderer *sdlRenderer = NULL;

/********************RENDER TARGETS (FRAMEBUFFERS)********************/

typedef struct sdlFrameBuffer_st
{
    SDL_Texture *texture;
    int width;
    int height;
} sdlFrameBuffer_t;

static sdlFrameBuffer_t frameBufferTexture;
static sdlFrameBuffer_t light1DBuffer;
static sdlFrameBuffer_t occluderBuffer;
static sdlFrameBuffer_t lightBuffer;

// Screen render target
static SDL_Texture *screenTexture = NULL;

/********************FONT RENDERING********************/

static SDL_Texture *fontTextureSDL = NULL;
static int screenshotFrame = 0;
static qbool screenshotSaved = qfalse;

#define CAMERA_VIEW_HEIGHT 200.0f
#define LEVEL_MIN_X 0.0f
#define LEVEL_MIN_Y -20.0f
#define LEVEL_MAX_X 560.0f
#define LEVEL_MAX_Y 540.0f

static float clampCameraAxis(float value, float minimum, float maximum)
{
    if(maximum < minimum)
        return (minimum + maximum) * 0.5f;
    return MAX(minimum, MIN(value, maximum));
}

/* Dynamic follow camera: eases after the player, leans toward the cursor
 * (look-ahead, as in Hotline Miami) and slightly into the direction of
 * movement, and snaps on respawn teleports. */
#define CAMERA_FOLLOW_RATE   9.0f   /* 1/s: higher follows tighter */
#define CAMERA_LOOK_AHEAD    0.42f  /* fraction of view the cursor can pull */
#define CAMERA_MOVE_LEAD     0.35f  /* seconds of velocity to lead by */
#define CAMERA_SNAP_DISTANCE 120.0f

static float cameraFocusX, cameraFocusY;
static float cameraLastPlayerX, cameraLastPlayerY;
static float cameraLeadX, cameraLeadY;
static bool cameraHasFocus;
static Uint64 cameraLastTicks;
static float cameraShake;

static void addCameraShake(float amount)
{
    cameraShake = MAX(cameraShake, amount);
}

static void updatePlayerCamera(int outputWidth, int outputHeight)
{
    float aspect = outputHeight > 0 ?
        (float)outputWidth / (float)outputHeight : 1.0f;

    /* SHADOWHUNT_OVERVIEW frames the whole harbor, for layout review. */
    if(SDL_getenv("SHADOWHUNT_OVERVIEW") != NULL) {
        cameraRect.h = LEVEL_MAX_Y - LEVEL_MIN_Y;
        cameraRect.w = cameraRect.h * aspect;
        cameraRect.x = (LEVEL_MIN_X + LEVEL_MAX_X - cameraRect.w) * 0.5f;
        cameraRect.y = LEVEL_MIN_Y;
        return;
    }

    cameraRect.h = CAMERA_VIEW_HEIGHT;
    cameraRect.w = CAMERA_VIEW_HEIGHT * aspect;

    Uint64 ticks = SDL_GetTicks();
    float dt = cameraLastTicks == 0 ? 0.016f : (ticks - cameraLastTicks) / 1000.0f;
    dt = MAX(0.0f, MIN(dt, 0.1f));
    cameraLastTicks = ticks;

    VectorEntity *owner = stealth_localPlayer();
    if(owner == NULL) {
        cameraHasFocus = false;
        cameraRect.x = LEVEL_MIN_X;
        cameraRect.y = LEVEL_MIN_Y;
        return;
    }

    float px = owner->pos.x, py = owner->pos.y;
    float moveX = px - cameraLastPlayerX, moveY = py - cameraLastPlayerY;
    bool teleported = !cameraHasFocus ||
        moveX * moveX + moveY * moveY > CAMERA_SNAP_DISTANCE * CAMERA_SNAP_DISTANCE;
    cameraLastPlayerX = px;
    cameraLastPlayerY = py;

    /* Movement lead, smoothed so direction changes do not jerk the view. */
    float velX = dt > 0 && !teleported ? moveX / dt : 0;
    float velY = dt > 0 && !teleported ? moveY / dt : 0;
    float leadBlend = 1.0f - expf(-4.0f * dt);
    cameraLeadX += (velX * CAMERA_MOVE_LEAD - cameraLeadX) * leadBlend;
    cameraLeadY += (velY * CAMERA_MOVE_LEAD - cameraLeadY) * leadBlend;

    /* Cursor look-ahead: offset from the window centre, so it cannot feed
     * back into itself as the camera moves. */
    float mouseX = 0.5f, mouseY = 0.5f;
    PlayerData *player = stealth_player(owner);
    bool canAct = player != NULL && owner->health > 0 && !player->spectating &&
                  SDL_getenv("SHADOWHUNT_TEST_AIM") == NULL;
    if(canAct)
        cl_getMouse(&mouseX, &mouseY);
    mouseX = MAX(0.0f, MIN(1.0f, mouseX));
    mouseY = MAX(0.0f, MIN(1.0f, mouseY));
    float lookX = (mouseX - 0.5f) * cameraRect.w * CAMERA_LOOK_AHEAD;
    float lookY = (mouseY - 0.5f) * cameraRect.h * CAMERA_LOOK_AHEAD;

    float targetX = px + lookX + cameraLeadX;
    float targetY = py + lookY + cameraLeadY;
    if(teleported) {
        cameraFocusX = targetX;
        cameraFocusY = targetY;
        cameraLeadX = cameraLeadY = 0;
        cameraHasFocus = true;
    } else {
        float blend = 1.0f - expf(-CAMERA_FOLLOW_RATE * dt);
        cameraFocusX += (targetX - cameraFocusX) * blend;
        cameraFocusY += (targetY - cameraFocusY) * blend;
    }

    /* Never let the player leave a safe margin of the screen. */
    float marginX = cameraRect.w * 0.38f, marginY = cameraRect.h * 0.36f;
    cameraFocusX = MAX(px - marginX, MIN(px + marginX, cameraFocusX));
    cameraFocusY = MAX(py - marginY, MIN(py + marginY, cameraFocusY));

    cameraRect.x = cameraFocusX - cameraRect.w * 0.5f;
    cameraRect.y = cameraFocusY - cameraRect.h * 0.5f;
    cameraRect.x = clampCameraAxis(cameraRect.x, LEVEL_MIN_X,
                                   LEVEL_MAX_X - cameraRect.w);
    cameraRect.y = clampCameraAxis(cameraRect.y, LEVEL_MIN_Y,
                                   LEVEL_MAX_Y - cameraRect.h);

    /* Trauma-style shake that decays quickly. */
    if(cameraShake > 0.01f) {
        float t = ticks * 0.001f;
        cameraRect.x += sinf(t * 91.0f) * cameraShake;
        cameraRect.y += cosf(t * 73.0f) * cameraShake;
        cameraShake *= expf(-10.0f * dt);
    }
}

void rect2xywh(rect2_t *r1, float x, float y, float w, float h) {
    (*r1)[0] = MIN(x, x + w);
    (*r1)[1] = MIN(y, y + h);
    (*r1)[2] = fabsf(w);
    (*r1)[3] = fabsf(h);
}


/********************HELPER FUNCTIONS********************/

SDL_FPoint convertPointToWindowCoord(SDL_FPoint point) {
    return (SDL_FPoint) {
        (point.x - cameraRect.x) * engineParameters.toWindowRatioX,
        (point.y - cameraRect.y) * engineParameters.toWindowRatioY
    };
}

SDL_FPoint convertPointToWorldCoord(SDL_FPoint point) {
    return (SDL_FPoint) {
        (point.x * engineParameters.toWorldRatioX) + cameraRect.x,
        (point.y * engineParameters.toWorldRatioY) + cameraRect.y
    };
}

SDL_FRect convertRectToWindowCoord(SDL_FRect rect) {
    return (SDL_FRect) {
        (rect.x - cameraRect.x) * engineParameters.toWindowRatioX,
        (rect.y - cameraRect.y) * engineParameters.toWindowRatioY,
        rect.w * engineParameters.toWindowRatioX,
        rect.h * engineParameters.toWindowRatioY
    };
}

/**
 * Creates an SDL texture that can be used as a render target (framebuffer replacement)
 */
sdlFrameBuffer_t createSDLFrameBuffer(int width, int height)
{
    sdlFrameBuffer_t fb;
    fb.width = width;
    fb.height = height;
    
    fb.texture = SDL_CreateTexture(
        sdlRenderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width,
        height
    );
    
    if (!fb.texture)
    {
        printf("ERROR: Failed to create SDL framebuffer texture: %s\n", SDL_GetError());
    }
    else
    {
        // Enable blending for framebuffer texture
        SDL_SetTextureBlendMode(fb.texture, SDL_BLENDMODE_BLEND);
        printf("Created framebuffer: %dx%d\n", width, height);
    }
    
    return fb;
}

/**
 * Destroys an SDL framebuffer
 */
void destroySDLFrameBuffer(sdlFrameBuffer_t *fb)
{
    if (fb->texture)
    {
        SDL_DestroyTexture(fb->texture);
        fb->texture = NULL;
    }
}

/**
 * Sets the render target to the specified framebuffer
 */
void bindSDLFrameBuffer(sdlFrameBuffer_t *fb)
{
    if (fb && fb->texture)
    {
        SDL_SetRenderTarget(sdlRenderer, fb->texture);
    }
    else
    {
        // NULL = render to screen
        SDL_SetRenderTarget(sdlRenderer, NULL);
    }
}

/**
 * Converts world coordinates to screen coordinates based on camera position
 */
void worldToScreen(float worldX, float worldY, float *screenX, float *screenY)
{
    // float cellsize = GraphicsHandle.cellsize;
    // camera_t camera = GraphicsHandle.camera;

    *screenX = (worldX - cameraRect.x) * engineParameters.toWindowRatioX;
    *screenY = (worldY - cameraRect.y) * engineParameters.toWindowRatioY;
    
    // *screenX = (worldX - camera.window[0]) * cellsize;
    // *screenY = (worldY - camera.window[1]) * cellsize;
}

/**
 * Converts angle to degrees for SDL (SDL uses degrees, not radians)
 */
float radiansToDegrees(float radians)
{
    return radians * 180.0f / M_PI;
}

/********************TEXTURE LOADING********************/

/**
 * Loads a texture from PNG file using SDL_Image
 * Replaces the OpenGL loadTexture function
 * 
 * Note: texImg parameter is kept for compatibility with existing code structure,
 * but the actual loading uses IMG_Load directly which is more efficient.
 */
static bool isAbsoluteAssetPath(const char *path)
{
    return path && (path[0] == '/' || path[0] == '\\' ||
                    (path[0] && path[1] == ':'));
}

/* Resolve an asset path relative to the working directory, the executable's
 * directory, or its parent (CMake copies assets into the build root). The
 * caller owns the returned string (SDL_free). */
char *findAssetPath(const char *relative)
{
    char *candidate = NULL;
    FILE *probe = fopen(relative, "rb");
    if(probe != NULL || isAbsoluteAssetPath(relative)) {
        if(probe != NULL)
            fclose(probe);
        return SDL_strdup(relative);
    }

    const char *basePath = SDL_GetBasePath();
    if(basePath != NULL) {
        if(SDL_asprintf(&candidate, "%s%s", basePath, relative) >= 0) {
            probe = fopen(candidate, "rb");
            if(probe != NULL) {
                fclose(probe);
                return candidate;
            }
            SDL_free(candidate);
            candidate = NULL;
        }

        size_t baseLen = SDL_strlen(basePath);
        while(baseLen > 0 && (basePath[baseLen - 1] == '/' || basePath[baseLen - 1] == '\\'))
            baseLen--;
        while(baseLen > 0 && basePath[baseLen - 1] != '/' && basePath[baseLen - 1] != '\\')
            baseLen--;
        if(baseLen > 0 && SDL_asprintf(&candidate, "%.*s%s", (int)baseLen, basePath, relative) >= 0) {
            probe = fopen(candidate, "rb");
            if(probe != NULL) {
                fclose(probe);
                return candidate;
            }
            SDL_free(candidate);
        }
    }
    return SDL_strdup(relative);
}

SDL_Texture* loadTexture(char *bmp_path) {
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    const char *basePath = NULL;
    char *loadPath = NULL;
    char *parentPath = NULL;

    if (!bmp_path)
        return NULL;

    // Try the caller's path first, which supports source-root and build-root
    // launches. Then resolve relative assets beside the executable and one
    // directory above it (CMake also places assets in the build root).
    surface = IMG_Load(bmp_path);
    if (!surface && !isAbsoluteAssetPath(bmp_path)) {
        basePath = SDL_GetBasePath();
        if (basePath) {
            if (SDL_asprintf(&loadPath, "%s%s", basePath, bmp_path) >= 0)
                surface = IMG_Load(loadPath);
            SDL_free(loadPath);
            loadPath = NULL;

            if (!surface) {
                size_t baseLen = SDL_strlen(basePath);
                while (baseLen > 0 && (basePath[baseLen - 1] == '/' ||
                                       basePath[baseLen - 1] == '\\'))
                    baseLen--;
                while (baseLen > 0 && basePath[baseLen - 1] != '/' &&
                       basePath[baseLen - 1] != '\\')
                    baseLen--;
                if (baseLen > 0 && SDL_asprintf(&parentPath, "%.*s%s",
                                                (int)baseLen, basePath,
                                                bmp_path) >= 0)
                    surface = IMG_Load(parentPath);
                SDL_free(parentPath);
            }
        }
    }

    if(!surface) {
        SDL_Log("Couldn't load bitmap %s: %s", bmp_path, SDL_GetError());
        return NULL;
    }

    texture = SDL_CreateTextureFromSurface(engineParameters.renderer, surface);
    if(!texture) {
        SDL_Log("Couldn't create static texture: %s", SDL_GetError());
        return NULL;
    }
    SDL_DestroySurface(surface);
    return texture;
}

// void loadTextureSDL(const char *path, SDL_Texture *texImg, SDL_Texture **texSDL)
// {
//     printf("Loading texture: %s\n", path);
    
//     // Use IMG_Load directly - much more efficient than custom PNG reader
//     SDL_Surface *surface = IMG_Load(path);
    
//     if (!surface)
//     {
//         printf("ERROR: Failed to load image: %s\n%s\n", path, SDL_GetError());
//         return;
//     }
    
//     // Store dimensions in texImg for compatibility with existing code
//     if (texImg)
//     {
//         texImg->w = surface->w;
//         texImg->h = surface->h;
//         // Note: We don't fill texImg->data as it's not needed for SDL rendering
//         // If needed for other purposes, you can access surface->pixels before destroying
//     }
    
//     // Create texture from surface
//     *texSDL = SDL_CreateTextureFromSurface(sdlRenderer, surface);
    
//     if (!*texSDL)
//     {
//         printf("ERROR: Failed to create texture from surface: %s\n", SDL_GetError());
//         SDL_DestroySurface(surface);
//         return;
//     }
    
//     // Enable blending for transparency
//     SDL_SetTextureBlendMode(*texSDL, SDL_BLENDMODE_BLEND);
    
//     printf("Loaded texture successfully: %dx%d\n", surface->w, surface->h);
    
//     // Clean up surface (texture has its own copy)
//     SDL_DestroySurface(surface);
// }

/**
 * Loads an animated texture atlas
 * Creates individual textures for each frame instead of using texture arrays
 */
void loadAnimTextureSDL(const char *path, SDL_Texture *texImg, int row, int col, SDL_Texture ***texArrayOut, int *frameCount)
{
    printf("Loading animated texture: %s (%dx%d frames)\n", path, row, col);
    
    char *resolved = findAssetPath(path);
    SDL_Surface *atlasSurface = IMG_Load(resolved);
    SDL_free(resolved);
    
    if (!atlasSurface)
    {
        printf("ERROR: Failed to load animated texture: %s\n%s\n", path, SDL_GetError());
        return;
    }
    
    // Store atlas dimensions in texImg for compatibility
    // if (texImg)
    // {
    //     texImg->width = atlasSurface->w;
    //     texImg->height = atlasSurface->h;
    // }
    
    float frameWidth = atlasSurface->w / (float)col;
    float frameHeight = atlasSurface->h / (float)row;
    int totalFrames = row * col;
    
    printf("Frame size: %.0fx%.0f pixels, Total frames: %d\n", frameWidth, frameHeight, totalFrames);
    
    // Allocate array of texture pointers
    SDL_Texture **texArray = (SDL_Texture **)zidmalloc(GENERALZONE, sizeof(SDL_Texture *) * totalFrames);
    
    // Extract each frame and create individual textures
    for (int r = 0; r < row; r++)
    {
        for (int c = 0; c < col; c++)
        {
            int frameIndex = r * col + c;
            
            // Create surface for this frame
            SDL_Surface *frameSurface = SDL_CreateSurface(
                (int)frameWidth,
                (int)frameHeight,
                atlasSurface->format
            );
            
            if (!frameSurface)
            {
                printf("ERROR: Failed to create frame surface for frame %d: %s\n", 
                       frameIndex, SDL_GetError());
                continue;
            }
            
            // Define source rectangle in atlas
            SDL_Rect srcRect;
            srcRect.x = c * (int)frameWidth;
            srcRect.y = r * (int)frameHeight;
            srcRect.w = (int)frameWidth;
            srcRect.h = (int)frameHeight;
            
            // Blit (copy) this portion of the atlas to the frame surface
            if (!SDL_BlitSurface(atlasSurface, &srcRect, frameSurface, NULL))
            {
                printf("ERROR: Failed to blit frame %d: %s\n", frameIndex, SDL_GetError());
                SDL_DestroySurface(frameSurface);
                continue;
            }
            
            // Create texture from frame surface
            texArray[frameIndex] = SDL_CreateTextureFromSurface(sdlRenderer, frameSurface);
            SDL_DestroySurface(frameSurface);
            
            if (texArray[frameIndex])
            {
                SDL_SetTextureBlendMode(texArray[frameIndex], SDL_BLENDMODE_BLEND);
            }
            else
            {
                printf("ERROR: Failed to create texture for frame %d: %s\n", 
                       frameIndex, SDL_GetError());
            }
        }
    }
    
    // Clean up atlas surface
    SDL_DestroySurface(atlasSurface);
    
    *texArrayOut = texArray;
    *frameCount = totalFrames;
    
    printf("Successfully created animated texture with %d frames\n", totalFrames);
}

/********************FRAMEBUFFER INITIALIZATION********************/

void createTextureBufferSDL()
{
    frameBufferTexture = createSDLFrameBuffer(
        (int)engineParameters.windowWidth,
        (int)engineParameters.windowHeight
    );
}

void createLightFrameBufferSDL()
{
    int lightSize = 512;
    int occluder = 256;
    
    light1DBuffer = createSDLFrameBuffer(lightSize, 1);
    occluderBuffer = createSDLFrameBuffer(occluder, occluder);
    lightBuffer = createSDLFrameBuffer(
        (int)engineParameters.windowWidth,
        (int)engineParameters.windowHeight
    );
}

/********************JSON LOADING********************/

void getTexName(char *texfile, char *texname)
{
    strcpy(texname, texfile);
    for (char *p = texname; *p; p++)
    {
        if (*p == '.')
        {
            *p = '\0';
            return;
        }
    }
}

char **getDemandTexList(cJSON *jsonDemTex)
{
    int lines, bufsize;
    char **demandTexList;
    char *buf;
    char *jstr;
    int len;
    
    lines = cJSON_GetArraySize(jsonDemTex);
    bufsize = 0;
    
    // printf("Reading texture demand, total count: %d\n", lines);
    
    for (int i = 0; i < lines; i++)
    {
        jstr = cJSON_GetStringValue(cJSON_GetArrayItem(jsonDemTex, i));
        bufsize += strlen(jstr) + 1;
    }
    
    char *alloc = (char *)zidmalloc(GENERALZONE, bufsize + sizeof(demandTexList) * lines);
    demandTexList = (char **)alloc;
    buf = (char *)demandTexList + sizeof(demandTexList) * lines;
    
    for (int i = 0; i < lines; i++)
    {
        jstr = cJSON_GetStringValue(cJSON_GetArrayItem(jsonDemTex, i));
        len = strlen(jstr) + 1;
        strcpy(buf, jstr);
        demandTexList[i] = buf;
        if (i != lines - 1)
            buf += len;
    }
    
    // printf("done demandTexList\n");
    return demandTexList;
}


void loadTextureAreas(cJSON *jsonTexAreas)
{
    int lines;
    cJSON *jsonTexAreaList, *jsonTexArea;
    textureRegion_t *texRegList;
    // int* texIDList;
    // unsigned int* VAOList;
    char *texName;
    float x, y, w, h;
    float u1, v1, u2, v2;
    int VAO;

    lines = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonTexAreas, "size"));
    jsonTexAreaList = cJSON_GetObjectItemCaseSensitive(jsonTexAreas, "object");
    texRegList = (textureRegion_t *)zidmalloc(GENERALZONE, sizeof(textureRegion_t) * lines );
    // texIDList = (int *)zidmalloc(GENERALZONE, sizeof(int) * lines);
    // VAOList = (unsigned int*) zidmalloc(GENERALZONE, sizeof(unsigned int) * lines);
   
    printf("checking lines %d \n", lines);
    int voff = 0;
    for (int i = 0; i < lines; i++)
    {
        jsonTexArea = cJSON_GetArrayItem(jsonTexAreaList, i);
        texName = cJSON_GetStringValue(cJSON_GetArrayItem(jsonTexArea, 0));

        x = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 1));
        y = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 2));
        w = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 3));
        h = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 4));

        u1 = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 5));
        v1 = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 6));
        u2 = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 7));
        v2 = cJSON_GetNumberValue(cJSON_GetArrayItem(jsonTexArea, 8));
        
        voff = 0;

        rect2xywh(&texRegList[i].area, x, y, w, h);

        texRegList[i].xyList[0] = x;
        texRegList[i].xyList[1] = y;
        texRegList[i].xyList[2] = x;
        texRegList[i].xyList[3] = y+h;
        texRegList[i].xyList[4] = x+w;
        texRegList[i].xyList[5] = y+h;
        texRegList[i].xyList[6] = x+w;
        texRegList[i].xyList[7] = y;

        // texRegList[i].uvList[0] = 0;
        // texRegList[i].uvList[1] = 0;
        // texRegList[i].uvList[2] = 0;
        // texRegList[i].uvList[3] = 1;
        // texRegList[i].uvList[4] = 1;
        // texRegList[i].uvList[5] = 1;
        // texRegList[i].uvList[6] = 1;
        // texRegList[i].uvList[7] = 0;

        texRegList[i].uvList[0] = u1;
        texRegList[i].uvList[1] = v1;
        texRegList[i].uvList[2] = u1;
        texRegList[i].uvList[3] = v2;
        texRegList[i].uvList[4] = u2;
        texRegList[i].uvList[5] = v2;
        texRegList[i].uvList[6] = u2;
        texRegList[i].uvList[7] = v1;

        texRegList[i].colorList[0].r = texRegList[i].colorList[0].g = texRegList[i].colorList[0].b = texRegList[i].colorList[0].a = 1.0f;
        texRegList[i].colorList[1].r = texRegList[i].colorList[1].g = texRegList[i].colorList[1].b = texRegList[i].colorList[1].a = 1.0f;
        texRegList[i].colorList[2].r = texRegList[i].colorList[2].g = texRegList[i].colorList[2].b = texRegList[i].colorList[2].a = 1.0f;
        texRegList[i].colorList[3].r = texRegList[i].colorList[3].g = texRegList[i].colorList[3].b = texRegList[i].colorList[3].a = 1.0f;
        
        texRegList[i].indexList[0] = 0;
        texRegList[i].indexList[1] = 1;
        texRegList[i].indexList[2] = 2;
        texRegList[i].indexList[3] = 0;
        texRegList[i].indexList[4] = 2;
        texRegList[i].indexList[5] = 3;

        // texRegList[i].vert[0 + voff] = x;
        // texRegList[i].vert[1 + voff] = y + h;
        // texRegList[i].vert[2 + voff] = 0;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = 1;
        // texRegList[i].vert[1 + voff] = 1;
        // texRegList[i].vert[2 + voff] = 1;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = u1;
        // texRegList[i].vert[1 + voff] = v2;
        // voff += 2;


        // texRegList[i].vert[0 + voff] = x + w;
        // texRegList[i].vert[1 + voff] = y + h;
        // texRegList[i].vert[2 + voff] = 0;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = 1;
        // texRegList[i].vert[1 + voff] = 1;
        // texRegList[i].vert[2 + voff] = 1;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = u2;
        // texRegList[i].vert[1 + voff] = v2;
        // voff += 2;


        // texRegList[i].vert[0 + voff] = x + w;
        // texRegList[i].vert[1 + voff] = y;
        // texRegList[i].vert[2 + voff] = 0;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = 1;
        // texRegList[i].vert[1 + voff] = 1;
        // texRegList[i].vert[2 + voff] = 1;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = u2;
        // texRegList[i].vert[1 + voff] = v1;
        // voff += 2;

        texRegList[i].texID = s2imap_get(TexRegHandle.texNameMap, texName);
        // VAO = createVAO(texRegList[i].vert, indices, VERTSIZE * sizeof(float), sizeof(indices));
        // VAOList[i] = VAO;
    }

    // TexRegHandle.VAOList = VAOList;
    // TexRegHandle.texIDList = texIDList;
    TexRegHandle.texRegList = texRegList;
    TexRegHandle.texRegCount = lines;
}

/**
 * Initializes textures from JSON configuration
 * Note: SDL version stores SDL_Texture pointers instead of OpenGL texture IDs
 */
void initTexturesSDL(int isClient)
{
    char *fbuf;
    char chartemp[256], *tempc;
    int texDemCount;
    const char *levelFile = "res//";
    
    char *levelPath = findAssetPath("levels/level.json");
    fbuf = getFileString(levelPath, TEMPORARYZONE);
    SDL_free(levelPath);
    if(fbuf == NULL)
        com_error(ERR_FATAL, "missing levels/level.json; run from the repository or build directory\n");
    
    cJSON *json = cJSON_Parse(fbuf);
    
    cJSON *texdemand = cJSON_GetObjectItemCaseSensitive(json, "texture_demand");
    char **demTexList = getDemandTexList(texdemand);
    texDemCount = cJSON_GetArraySize(texdemand);
    
    TexRegHandle.texNameMap = s2imap_create(GENERALZONE);
    TexImgHandle.texImgCount = 0;
    
    for (int i = 0; i < texDemCount; i++)
    {
        getTexName(demTexList[i], chartemp);
        s2imap_put(TexRegHandle.texNameMap, chartemp, i);
        // if(strcmp(chartemp, "crate_wooden_2.png")) {
        //     printf("checking index %d \n", i)
        // }
    }
    
    if (isClient)
    {
        TexImgHandle.texImgCount = texDemCount;
        TexImgHandle.texImgList = (SDL_Texture **)zidmalloc(GENERALZONE,
                                                               sizeof(SDL_Texture **) * texDemCount);
        
        // For SDL, we need to store SDL_Texture pointers as well
        // We'll cast the texNameList to store texture pointers
        // TexImgHandle.texNameList = (unsigned int *)zidmalloc(GENERALZONE,
        //                                                      sizeof(unsigned int *) * texDemCount);
        
        for (int i = 0; i < texDemCount; i++)
        {
            strcpy(chartemp, levelFile);
            tempc = chartemp + strlen(levelFile);
            strcpy(tempc, demTexList[i]);
            
            SDL_Texture *sdlTex = NULL;
            // loadTextureSDL(chartemp, &TexImgHandle.texImgList[i], &sdlTex);
            // printf("before load texture \n");
            sdlTex = loadTexture(chartemp);
            printf("checking file name %s %d \n", chartemp, i);
            // printf("after load texture \n");

            SDL_SetTextureScaleMode(sdlTex, SDL_SCALEMODE_NEAREST); 

            TexImgHandle.texImgList[i] = sdlTex;
            
            // Store texture pointer in the nameList (reinterpreted as pointer storage)
            // ((SDL_Texture **)TexImgHandle.texNameList)[i] = sdlTex;
        }
    }

    cJSON *jsonTexAreas = cJSON_GetObjectItemCaseSensitive(json, "texture");
    loadTextureAreas(jsonTexAreas);
    
    cJSON_free(json);
    zidfree(fbuf);
}

// extern void sprite_init();
// extern void sprite_add(char *spriteName, int id, int type);
// extern int sprite_getID(char *spriteName, int type);

/**
 * Initializes sprite system from JSON configuration
 */
void initSpritesSDL(int isClient)
{
    char *fbuf;
    cJSON *jsonSpriteList;
    cJSON *jsonSpriteFolder;
    cJSON *jsonSpriteObj;
    cJSON *jsonAnimFileName;
    cJSON *jsonNumVal;
    
    char *spriteFolder;
    char *spriteFileName;
    char *spriteObjName;
    char spriteFilePath[128];
    char *writePath;
    char *pathSep = "//";
    int spriteLen;
    int i = 0;
    int row;
    int col;
    
    char *loadPath;
    //sboat_small_vertical
    loadPath = findAssetPath("levels/config/sprite.json");
    printf("sprite config %s \n", loadPath);

    fbuf = getFileString(loadPath, TEMPORARYZONE);
    SDL_free(loadPath);
    if(fbuf == NULL)
        com_error(ERR_FATAL, "missing levels/config/sprite.json; run from the repository or build directory\n");
    
    cJSON *json = cJSON_Parse(fbuf);
    
    // cJSON *jsonTexAreas = cJSON_GetObjectItemCaseSensitive(json, "texture");
    // int lines = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonTexAreas, "size"));
    // printf("checking texture lines %d \n", lines);

    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);
    printf("spriteLen %d \n", spriteLen);
    
    jsonSpriteFolder = cJSON_GetObjectItemCaseSensitive(jsonSpriteList, "folder");
    spriteFolder = cJSON_GetStringValue(jsonSpriteFolder);

    printf("checking sprite folder %s \n", spriteFolder);
    
    SpriteHandle.texImgList = (SDL_Texture **)zidmalloc(PERMANENTZONE, sizeof(SDL_Texture*) * spriteLen);
    // SpriteHandle.texNameList = (unsigned int *)zidmalloc(PERMANENTZONE, sizeof(SDL_Texture *) * spriteLen);
    
    sprite_init();
    
    jsonSpriteObj = NULL;
    
    cJSON_ArrayForEach(jsonSpriteObj, jsonSpriteList)
    {
        spriteObjName = jsonSpriteObj->string;
        spriteFileName = cJSON_GetStringValue(jsonSpriteObj);
        
        if (strcmp(spriteObjName, "folder") == 0)
            continue;
        
        writePath = spriteFilePath;
        strcpy(writePath, spriteFolder);
        
        writePath += strlen(spriteFolder);
        strcpy(writePath, pathSep);
        
        writePath += strlen(pathSep);
        strcpy(writePath, spriteFileName);
        
        printf("Reading sprite file: %s\n", spriteFilePath);
        
        if (isClient)
        {
            SDL_Texture *sdlTex = NULL;
            // loadTextureSDL(spriteFilePath, &SpriteHandle.texImgList[i], &sdlTex);
            sdlTex = loadTexture(spriteFilePath);

            SDL_SetTextureScaleMode(sdlTex, SDL_SCALEMODE_NEAREST); 

            SpriteHandle.texImgList[i] = sdlTex;
        }
        
        sprite_add(spriteObjName, i, SPRITE_TYPE_STATIC);
        
        i++;
    }
    
    // Load animated sprites
    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "animated_sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);
    
    jsonSpriteFolder = cJSON_GetObjectItemCaseSensitive(jsonSpriteList, "folder");
    spriteFolder = cJSON_GetStringValue(jsonSpriteFolder);
    
    AnimSpriteHandle.animSpriteList = (animatedSpriteImage_t *)zidmalloc(PERMANENTZONE,
                                                                          sizeof(animatedSpriteImage_t) * spriteLen);
    
    jsonSpriteObj = NULL;
    i = 0;
    
    cJSON_ArrayForEach(jsonSpriteObj, jsonSpriteList)
    {
        spriteObjName = jsonSpriteObj->string;
        jsonAnimFileName = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "file");
        
        spriteFileName = cJSON_GetStringValue(jsonAnimFileName);
        
        if (strcmp(spriteObjName, "folder") == 0)
            continue;
        
        writePath = spriteFilePath;
        strcpy(writePath, spriteFolder);
        
        writePath += strlen(spriteFolder);
        strcpy(writePath, pathSep);
        
        writePath += strlen(pathSep);
        strcpy(writePath, spriteFileName);
        
        printf("Reading animated sprite file: %s\n", spriteFilePath);
        
        jsonNumVal = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "row");
        row = jsonNumVal ? cJSON_GetNumberValue(jsonNumVal) : 1;
        jsonNumVal = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "col");
        col = jsonNumVal ? cJSON_GetNumberValue(jsonNumVal) : 1;
        if(row < 1) row = 1;
        if(col < 1) col = 1;
        
        AnimSpriteHandle.animSpriteList[i].row = row;
        AnimSpriteHandle.animSpriteList[i].col = col;
        AnimSpriteHandle.animSpriteList[i].total = row * col;
        
        if (isClient)
        {
            SDL_Texture **frameTextures = NULL;
            int frameCount = 0;

            loadAnimTextureSDL(spriteFilePath, NULL, row, col,
                               &frameTextures, &frameCount);
            AnimSpriteHandle.animSpriteList[i].texImage = frameTextures;
            AnimSpriteHandle.animSpriteList[i].total = frameCount;
        }
        
        sprite_add(spriteObjName, i, SPRITE_TYPE_ANIM);
        
        i++;
    }
    AnimSpriteHandle.imgCount = i;
    
    cJSON_free(json);
    zidfree(fbuf);
}

/********************RENDERING FUNCTIONS********************/

// void renderSprite(Sprite *sprite) {
//     // SDL_FRect dst_rect;
//     // dst_rect.x = (100.0f * fighter_scale);
//     // dst_rect.y = 0;
//     // // dst_rect.x = 52;
//     // // dst_rect.y = 71;
//     // dst_rect.w = texture_width * fighter_scale;
//     // dst_rect.h = texture_height * fighter_scale;
//     // if(test == 0) {
//     //     printf("%f, %f, %f, %f \n", dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h);
//     //     test = 1;
//     // }
//     // center.x = texture_width / 2.0f;
//     // center.y = texture_height / 2.0f;
//     Sprite tempSprite;
//     tempSprite.rect = convertRectToWindowCoord(sprite->rect);
//     // tempSprite.center = convertPointToWindowCoord(sprite->center);
//     tempSprite.center = sprite->center;
//     tempSprite.center.x *= engineParameters.toWindowRatioX;
//     tempSprite.center.y *= engineParameters.toWindowRatioY;
//     // if(sprite->textureID == 3) {
//     //     printf("checking sprite %f %f %f %f \n", tempSprite.rect.x, tempSprite.rect.y, tempSprite.center.x, tempSprite.center.y);
//     // }
//     // tempSprite.rect.x = 0;
//     // tempSprite.rect.y = 0;
//     // tempSprite.center.x = 0;
//     // tempSprite.center.y = 0;
//     // printf("checking coord: %f, %f, %f, %f, %f, %f\n", tempSprite.rect.x, tempSprite.rect.y, tempSprite.rect.w, tempSprite.rect.h, sprite->rect.x, sprite->rect.y);
//     // printf("checking fraction %f \n", (WINDOW_WIDTH/ WORLD_SCREEN_WIDTH));
//     SDL_RenderTextureRotated(engineParameters.renderer, textureList[sprite->textureID], NULL, &tempSprite.rect, sprite->rotate, &tempSprite.center, SDL_FLIP_NONE);
//     // SDL_RenderTexture(engineParameters.renderer, textureList[sprite->textureID], NULL, &tempSprite.rect);
// }

/**
 * Draws a sprite at specified world position with rotation
 * Replaces OpenGL drawSprite function
 */
void drawSpriteSDL(SDL_Texture *texture, float x, float y, float rect[4], float angle)
{
    // float cellsize = GraphicsHandle.cellsize;
    // camera_t camera = GraphicsHandle.camera;
    
    // SDL_Texture *texture = SpriteHandle.texImgList[spriteID];
    
    // if (!texture)
    // {
    //     printf("ERROR: Invalid sprite texture ID %d\n", spriteID);
    //     return;
    // }
    
    // Convert world coordinates to screen coordinates
    // float screenX, screenY;
    // worldToScreen(x, y, &screenX, &screenY);
    
    // Calculate destination rectangle in screen space
    SDL_FRect destRect;
    // destRect.x = x + rect[0];
    // destRect.y = y + rect[1];
    destRect.x = (x - cameraRect.x);
    destRect.y = (y - cameraRect.y);
    destRect.w = rect[2];
    destRect.h = rect[3];
    // destRect.x = 0;
    // destRect.y = 0;
    // destRect.w = 100;
    // destRect.h = 100;

    destRect.x *= engineParameters.toWindowRatioX;
    destRect.y *= engineParameters.toWindowRatioY;
    destRect.w *= engineParameters.toWindowRatioX;
    destRect.h *= engineParameters.toWindowRatioY;
    // float cellsize = 10;
    // destRect.x *= cellsize;
    // destRect.y *= cellsize;
    // destRect.w *= cellsize;
    // destRect.x *= cellsize;
    
    // if(spriteID == 0) 
    // printf("destRect %f %f %f %f\n", destRect.x, destRect.y, destRect.w, destRect.h);
    // Rotation center (relative to destination rectangle)
    // printf("checking size %d %d\n", texture->w, texture->h);

    SDL_FPoint center;
    center.x = 0;
    center.y = 0;
    // center.x = destRect.w / 2.0f;
    // center.y = destRect.h / 2.0f;
    
    // Convert angle from radians to degrees
    // float angleDegrees = radiansToDegrees(angle);
    float angleDegrees = angle;
    
    // Render the texture with rotation
    SDL_RenderTextureRotated(sdlRenderer, texture, NULL, &destRect,
                            angleDegrees, &center, SDL_FLIP_NONE);
}

/**
 * Draws an animated sprite
 * Replaces OpenGL drawAnimSprite function
 */
void drawAnimSpriteSDL(animatedSprite_t *entSprite)
{
    // float cellsize = GraphicsHandle.cellsize;
    // camera_t camera = GraphicsHandle.camera;
    
    animatedSpriteImage_t *animSpriteImage;
    animSpriteImage = &AnimSpriteHandle.animSpriteList[entSprite->texID];
    
    // Get frame texture array
    SDL_Texture **frameTextures = animSpriteImage->texImage;
    
    if (!frameTextures)
    {
        printf("ERROR: Invalid animated sprite texture\n");
        return;
    }
    
    // Calculate current frame index
    int frameIndex = (int)(entSprite->curSprite * animSpriteImage->total) % (int)animSpriteImage->total;
    SDL_Texture *currentFrame = frameTextures[frameIndex];
    
    if (!currentFrame)
    {
        printf("ERROR: Invalid frame texture at index %d\n", frameIndex);
        return;
    }
    
    float x = entSprite->pos[0];
    float y = entSprite->pos[1];
    
    // Convert world coordinates to screen coordinates
    float screenX, screenY;
    worldToScreen(x, y, &screenX, &screenY);
    
    // Calculate destination rectangle
    SDL_FRect destRect;
    destRect.x = screenX + entSprite->rect[0] * engineParameters.toWindowRatioX;
    destRect.y = screenY + entSprite->rect[1] * engineParameters.toWindowRatioY;
    destRect.w = entSprite->rect[2] * engineParameters.toWindowRatioX;
    destRect.h = entSprite->rect[3] * engineParameters.toWindowRatioY;
    
    // Rotation center
    SDL_FPoint center;
    center.x = destRect.w / 2.0f;
    center.y = destRect.h / 2.0f;
    
    float angleDegrees = radiansToDegrees(entSprite->angle);
    
    SDL_RenderTextureRotated(sdlRenderer, currentFrame, NULL, &destRect,
                            angleDegrees, &center, SDL_FLIP_NONE);
}

/**
 * Simplified lighting system for SDL
 * Note: Full shadow mapping requires shader support which SDL doesn't have by default
 * This is a simplified version
 */
// void renderLightSDL()
// {
//     // For SDL, we'll need to implement a simplified lighting system
//     // or use SDL_gpu for shader support in the future
    
//     // Placeholder: render occluders to buffer
//     bindSDLFrameBuffer(&occluderBuffer);
//     SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 0);
//     SDL_RenderClear(sdlRenderer);
    
//     rect2_t wall;
//     rect2_t bound;
    
//     for (int i = 0; i < world.worldWallSize; i++)
//     {
//         rect2set(wall, world.worldWallArray[i].rect);
        
//         bound[0] = bound[1] = 0;
//         bound[2] = wall[2];
//         bound[3] = wall[3];
        
//         drawSpriteSDL( TexImgHandle.texImgList[0], wall[0], wall[1], bound, 0);
//     }
    
//     // Bind back to main framebuffer
//     bindSDLFrameBuffer(&frameBufferTexture);
// }

/**
 * Renders text using a font texture atlas
 */
void renderFontSDL(const char *text, float x, float y)
{
    if (!fontTextureSDL || !text)
        return;
    
    const float charWidth = 14.0f;
    const float charHeight = 30.0f;
    const int atlasColumns = 32;
    const int firstGlyph = 32;
    float textureWidth = 0;
    float textureHeight = 0;
    if(!SDL_GetTextureSize(fontTextureSDL, &textureWidth, &textureHeight))
        return;
    
    for (int i = 0; text[i] != '\0'; i++)
    {
        char ch = text[i];
        
        if(ch < firstGlyph || ch > 126)
            continue;

        int charIndex = ch - firstGlyph;
        int glyphColumn = charIndex % atlasColumns;
        int glyphRow = charIndex / atlasColumns;
        
        // Calculate source rectangle in font atlas
        SDL_FRect srcRect;
        srcRect.w = textureWidth / 32.0f;
        srcRect.h = textureHeight / 3.0f;
        srcRect.x = glyphColumn * srcRect.w;
        srcRect.y = glyphRow * srcRect.h;
        
        // Calculate destination rectangle
        SDL_FRect destRect;
        destRect.x = x + (charWidth * i);
        destRect.y = y;
        destRect.w = charWidth;
        destRect.h = charHeight;
        
        SDL_RenderTexture(sdlRenderer, fontTextureSDL, &srcRect, &destRect);
    }
}

void renderWorldTexture(textureRegion_t *texReg) {
    for(int i = 0; i < 8; i+=2) {
        // texReg->xyList[i]
        // float screenX, screenY;
        // worldToScreen(texReg->xyList[i], texReg->xyList[i+1], &screenX, &screenY);
        texReg->renderXYList[i] = (texReg->xyList[i] - cameraRect.x)*engineParameters.toWindowRatioX;
        texReg->renderXYList[i+1] = (texReg->xyList[i+1] - cameraRect.y)*engineParameters.toWindowRatioY;
    }
    SDL_RenderGeometryRaw(sdlRenderer,
        TexImgHandle.texImgList[texReg->texID],
        texReg->renderXYList,
        sizeof(float) * 2,
        texReg->colorList,
        sizeof(SDL_FColor),
        texReg->uvList,
        sizeof(float)*2,
        8,
        texReg->indexList,
        6,
        1);
}

/********************STEALTH PRESENTATION********************/

static SDL_Texture *lightTexture = NULL;
static SDL_Texture *darknessTexture = NULL;
static int darknessWidth = 0, darknessHeight = 0;
static SDL_BlendMode eraseBlendMode = SDL_BLENDMODE_INVALID;
static bool eraseSupported = false;
static float hudScale = 1.0f;

typedef struct { Uint8 r, g, b; } rgb_t;

/* Hotline Miami-flavoured lamp colours, cycled across the level's lights. */
static const rgb_t lampPalette[] = {
    {255, 176, 92}, {255, 70, 170}, {70, 220, 255}, {176, 110, 255}, {255, 214, 120}
};

static SDL_Texture *glowTexture = NULL;

static SDL_Texture *createLightTexture(bool soft)
{
    const int size = 256;
    SDL_Surface *surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32);
    if(surface == NULL)
        return NULL;
    Uint8 *pixels = (Uint8 *)surface->pixels;
    for(int y = 0; y < size; y++) {
        for(int x = 0; x < size; x++) {
            float dx = (x + 0.5f) / (size * 0.5f) - 1.0f;
            float dy = (y + 0.5f) / (size * 0.5f) - 1.0f;
            float d = sqrtf(dx * dx + dy * dy);
            /* Flat core with a soft edge: players at the rim are still lit. */
            float a;
            if(soft) {
                /* Glow: bright centre fading smoothly to nothing. */
                a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
            } else {
                a = d >= 1.0f ? 0.0f : d < 0.72f ? 1.0f : 1.0f - (d - 0.72f) / 0.28f;
                a = a * a * (3.0f - 2.0f * a);
            }
            Uint8 *px = pixels + y * surface->pitch + x * 4;
            px[0] = px[1] = px[2] = 255;
            px[3] = (Uint8)(a * 255.0f);
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
    SDL_DestroySurface(surface);
    return texture;
}

static void ensureDarknessTexture(int width, int height)
{
    if(darknessTexture != NULL && darknessWidth == width && darknessHeight == height)
        return;
    if(darknessTexture != NULL)
        SDL_DestroyTexture(darknessTexture);
    darknessTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET, width, height);
    darknessWidth = width;
    darknessHeight = height;
    if(darknessTexture != NULL)
        SDL_SetTextureBlendMode(darknessTexture, SDL_BLENDMODE_BLEND);
}

static void worldCircleToScreen(float x, float y, float radius, SDL_FRect *out)
{
    float sx, sy;
    worldToScreen(x, y, &sx, &sy);
    out->w = radius * 2 * engineParameters.toWindowRatioX;
    out->h = radius * 2 * engineParameters.toWindowRatioY;
    out->x = sx - out->w * 0.5f;
    out->y = sy - out->h * 0.5f;
}

static void drawGlow(float x, float y, float radius, rgb_t color, Uint8 alpha)
{
    if(glowTexture == NULL)
        return;
    SDL_FRect dest;
    worldCircleToScreen(x, y, radius, &dest);
    SDL_SetTextureBlendMode(glowTexture, SDL_BLENDMODE_ADD);
    SDL_SetTextureColorMod(glowTexture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(glowTexture, alpha);
    SDL_RenderTexture(sdlRenderer, glowTexture, NULL, &dest);
    SDL_SetTextureAlphaMod(glowTexture, 255);
    SDL_SetTextureColorMod(glowTexture, 255, 255, 255);
}

static void eraseDarkness(float x, float y, float radius)
{
    SDL_FRect dest;
    worldCircleToScreen(x, y, radius, &dest);
    SDL_RenderTexture(sdlRenderer, lightTexture, NULL, &dest);
}

/* The mask: a near-black layer with soft holes cut out wherever light falls.
 * Hunters get a much darker mask than hiders, who need to read the map. */
static void renderDarkness(int outputWidth, int outputHeight, Uint8 darkness)
{
    if(lightTexture == NULL)
        return;
    ensureDarknessTexture(outputWidth, outputHeight);
    if(darknessTexture == NULL)
        return;

    SDL_SetRenderTarget(sdlRenderer, darknessTexture);
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(sdlRenderer, 4, 2, 14, darkness);
    SDL_RenderClear(sdlRenderer);

    if(eraseSupported) {
        SDL_SetTextureBlendMode(lightTexture, eraseBlendMode);
        SDL_SetTextureColorMod(lightTexture, 255, 255, 255);
        SDL_SetTextureAlphaMod(lightTexture, 255);
        for(int i = 0; i < world.lightCount; i++)
            eraseDarkness(world.lights[i].x, world.lights[i].y, world.lights[i].radius);
        for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
            VectorEntity *vecEnt = &vectorEntityList[i];
            if(vecEnt->active && stealth_isHunter(vecEnt))
                eraseDarkness(vecEnt->pos.x, vecEnt->pos.y, SH_LANTERN_RADIUS);
        }
    }

    SDL_SetRenderTarget(sdlRenderer, NULL);
    SDL_RenderTexture(sdlRenderer, darknessTexture, NULL, NULL);
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
}

/********************TEXT********************/

static void drawTextRaw(const char *text, float x, float y, float scale,
                        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if(!fontTextureSDL || !text)
        return;
    float textureWidth = 0, textureHeight = 0;
    if(!SDL_GetTextureSize(fontTextureSDL, &textureWidth, &textureHeight))
        return;
    float charWidth = 14.0f * scale, charHeight = 30.0f * scale;
    SDL_SetTextureColorMod(fontTextureSDL, r, g, b);
    SDL_SetTextureAlphaMod(fontTextureSDL, a);
    for(int i = 0; text[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)text[i];
        if(ch < 32 || ch > 126)
            continue;
        int index = ch - 32;
        SDL_FRect src = {(index % 32) * textureWidth / 32.0f,
                         (index / 32) * textureHeight / 3.0f,
                         textureWidth / 32.0f, textureHeight / 3.0f};
        SDL_FRect dest = {x + charWidth * i, y, charWidth, charHeight};
        SDL_RenderTexture(sdlRenderer, fontTextureSDL, &src, &dest);
    }
    SDL_SetTextureColorMod(fontTextureSDL, 255, 255, 255);
    SDL_SetTextureAlphaMod(fontTextureSDL, 255);
}

static void drawText(const char *text, float x, float y, float scale, rgb_t color, Uint8 alpha)
{
    scale *= hudScale;
    float shadow = 2.0f * scale;
    drawTextRaw(text, x + shadow, y + shadow, scale, 0, 0, 0, (Uint8)(alpha * 0.8f));
    drawTextRaw(text, x, y, scale, color.r, color.g, color.b, alpha);
}

static float textWidth(const char *text, float scale)
{
    return (float)strlen(text) * 14.0f * scale * hudScale;
}

static void drawTextCentered(const char *text, float centerX, float y, float scale,
                             rgb_t color, Uint8 alpha)
{
    drawText(text, centerX - textWidth(text, scale) * 0.5f, y, scale, color, alpha);
}

static void drawBar(float x, float y, float w, float h, float ratio, rgb_t color)
{
    ratio = MAX(0.0f, MIN(1.0f, ratio));
    SDL_FRect back = {x, y, w, h};
    SDL_FRect fill = {x, y, w * ratio, h};
    SDL_SetRenderDrawColor(sdlRenderer, 20, 12, 30, 200);
    SDL_RenderFillRect(sdlRenderer, &back);
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(sdlRenderer, &fill);
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 90);
    SDL_RenderRect(sdlRenderer, &back);
}

/********************ACTORS********************/

static void drawFrame(int animID, float cycle, float x, float y, const float rect[4],
                      float angle, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if(animID < 0 || animID >= AnimSpriteHandle.imgCount)
        return;
    animatedSpriteImage_t *image = &AnimSpriteHandle.animSpriteList[animID];
    if(image->texImage == NULL || image->total <= 0)
        return;
    int frameIndex = ((int)(cycle * image->total) % (int)image->total + (int)image->total) %
                     (int)image->total;
    SDL_Texture *frame = image->texImage[frameIndex];
    if(frame == NULL)
        return;

    float sx, sy;
    worldToScreen(x, y, &sx, &sy);
    SDL_FRect dest = {sx + rect[0] * engineParameters.toWindowRatioX,
                      sy + rect[1] * engineParameters.toWindowRatioY,
                      rect[2] * engineParameters.toWindowRatioX,
                      rect[3] * engineParameters.toWindowRatioY};
    SDL_FPoint center = {dest.w * 0.5f, dest.h * 0.5f};
    SDL_SetTextureColorMod(frame, r, g, b);
    SDL_SetTextureAlphaMod(frame, a);
    SDL_RenderTextureRotated(sdlRenderer, frame, NULL, &dest,
                             radiansToDegrees(angle), &center, SDL_FLIP_NONE);
    SDL_SetTextureColorMod(frame, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame, 255);
}

static int animLegs = -1, animHunter = -1, animHider = -1, animHiderDead = -1;
static int spritePellet = -1;

static void resolveSpriteIDs(void)
{
    static bool resolved;
    if(resolved)
        return;
    animLegs = sprite_getID("actor_legs", SPRITE_TYPE_ANIM);
    animHunter = sprite_getID("actor_torso_walk_machgun", SPRITE_TYPE_ANIM);
    animHider = sprite_getID("actor_stealth_standing", SPRITE_TYPE_ANIM);
    animHiderDead = sprite_getID("stealth_dead", SPRITE_TYPE_ANIM);
    spritePellet = sprite_getID("pickup_red_pellet", SPRITE_TYPE_STATIC);
    resolved = true;
}

static bool actorVisible(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    return vecEnt->active && player != NULL && !player->hiddenFromViewer &&
           !player->spectating;
}

static void drawActor(VectorEntity *vecEnt, Uint8 alpha)
{
    static const float legsRect[4] = {-6.4f, -6.4f, 12.8f, 12.8f};
    static const float torsoRect[4] = {-9.0f, -6.5f, 18.0f, 13.0f};
    static const float corpseRect[4] = {-10.0f, -5.0f, 20.0f, 10.0f};
    PlayerData *player = stealth_player(vecEnt);
    unsigned long now = getTimeMillis();
    float x = vecEnt->pos.x, y = vecEnt->pos.y;

    if(player->role == SH_ROLE_HIDER && vecEnt->health <= 0) {
        drawFrame(animHiderDead, 0, x, y, corpseRect, vecEnt->animSprite.angle,
                  255, 255, 255, alpha);
        return;
    }

    Uint8 r = 255, g = 255, b = 255;
    bool powered = player->powerUntil > now;
    bool frozen = player->frozenUntil > now;
    if(powered) {
        /* Red hot: a flickering ember tint. */
        float flicker = 0.75f + 0.25f * sinf(now * 0.025f);
        r = 255; g = (Uint8)(90 * flicker); b = (Uint8)(40 * flicker);
    } else if(frozen) {
        r = 130; g = 200; b = 255;
    } else if(player->role == SH_ROLE_HUNTER) {
        r = 255; g = 235; b = 210;
    }
    if(player->hitFlashUntil > now) {
        r = 255; g = 60; b = 60;
    }

    if(player->moving && !frozen)
        drawFrame(animLegs, player->walkCycle * 0.5f, x, y, legsRect, player->moveAngle,
                  r, g, b, alpha);
    int torso = player->role == SH_ROLE_HUNTER ? animHunter : animHider;
    float cycle = player->role == SH_ROLE_HUNTER && player->moving ? player->walkCycle : 0;
    drawFrame(torso, cycle, x, y, torsoRect, vecEnt->animSprite.angle, r, g, b, alpha);
}

static void drawRing(float x, float y, float radius, rgb_t color, Uint8 alpha)
{
    const int segments = 28;
    SDL_FPoint points[29];
    for(int i = 0; i <= segments; i++) {
        float a = (float)i / segments * 2.0f * (float)M_PI;
        worldToScreen(x + cosf(a) * radius, y + sinf(a) * radius, &points[i].x, &points[i].y);
    }
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, alpha);
    SDL_RenderLines(sdlRenderer, points, segments + 1);
}

static void drawPellets(void)
{
    unsigned long now = getTimeMillis();
    float pulse = 0.5f + 0.5f * sinf(now * 0.008f);
    for(int i = 0; i < world.pelletCount; i++) {
        if(!shGame.pelletActive[i])
            continue;
        float x = world.pellets[i].x, y = world.pellets[i].y;
        drawGlow(x, y, 14 + 4 * pulse, (rgb_t){255, 40, 40}, (Uint8)(150 + 80 * pulse));
        if(spritePellet >= 0 && SpriteHandle.texImgList[spritePellet] != NULL) {
            float size = 6 + pulse;
            float rect[4] = {0, 0, size, size};
            drawSpriteSDL(SpriteHandle.texImgList[spritePellet], x - size / 2, y - size / 2, rect, 0);
        } else {
            SDL_FRect dest;
            worldCircleToScreen(x, y, 3, &dest);
            SDL_SetRenderDrawColor(sdlRenderer, 255, 50, 50, 255);
            SDL_RenderFillRect(sdlRenderer, &dest);
        }
    }
}

static void drawTracers(void)
{
    unsigned long now = getTimeMillis();
    for(int i = 0; i < SH_MAX_SHOTS; i++) {
        sh_shot_t *shot = &shGame.shots[i];
        if(shot->time == 0 || now - shot->time > 160)
            continue;
        float life = 1.0f - (now - shot->time) / 160.0f;
        float x1, y1, x2, y2;
        worldToScreen(shot->x, shot->y, &x1, &y1);
        worldToScreen(shot->endX, shot->endY, &x2, &y2);
        SDL_SetRenderDrawColor(sdlRenderer, 255, 240, 150, (Uint8)(255 * life));
        for(int o = -1; o <= 1; o++) {
            SDL_RenderLine(sdlRenderer, x1 + o, y1, x2 + o, y2);
            SDL_RenderLine(sdlRenderer, x1, y1 + o, x2, y2 + o);
        }
        drawGlow(shot->x, shot->y, 10, (rgb_t){255, 210, 120}, (Uint8)(200 * life));
        drawGlow(shot->endX, shot->endY, 6, (rgb_t){255, 160, 80}, (Uint8)(200 * life));
    }
}

/********************HUD********************/

static const rgb_t HUD_PINK = {255, 64, 160};
static const rgb_t HUD_CYAN = {64, 224, 255};
static const rgb_t HUD_AMBER = {255, 190, 80};
static const rgb_t HUD_RED = {255, 60, 60};
static const rgb_t HUD_WHITE = {245, 240, 255};
static const rgb_t HUD_ICE = {150, 210, 255};

static void renderHUD(int w, int h)
{
    VectorEntity *local = stealth_localPlayer();
    PlayerData *player = local != NULL ? stealth_player(local) : NULL;
    unsigned long now = getTimeMillis();
    int role = stealth_localRole();
    char line[128];
    float pad = 18 * hudScale;
    float pulse = 0.5f + 0.5f * sinf(now * 0.006f);

    /* Role badge. */
    if(MATCH_STATE == MATCH_WAITING)
        drawText("WARM UP", pad, pad, 1.2f, HUD_AMBER, 255);
    else if(role == SH_ROLE_HUNTER)
        drawText("HUNTER", pad, pad, 1.4f, HUD_CYAN, 255);
    else if(role == SH_ROLE_HIDER)
        drawText("HIDER", pad, pad, 1.4f, HUD_PINK, 255);
    else
        drawText("SPECTATING", pad, pad, 1.2f, HUD_WHITE, 220);

    /* Round clock and head count. */
    if(MATCH_STATE == MATCH_RUNNING) {
        long ms = MAX(0, shGame.roundRemainingMs - (long)(now - shGame.lastUpdateAt));
        snprintf(line, sizeof(line), "%ld:%02ld", ms / 60000, (ms / 1000) % 60);
        drawTextCentered(line, w * 0.5f, pad, 1.4f, ms < 20000 ? HUD_RED : HUD_WHITE, 255);
        snprintf(line, sizeof(line), "HIDERS LEFT %d/%d", shGame.hidersAlive, shGame.hidersTotal);
        drawTextCentered(line, w * 0.5f, pad + 46 * hudScale, 0.8f, HUD_PINK, 230);
    }

    if(player != NULL && MATCH_STATE == MATCH_RUNNING && role == SH_ROLE_HUNTER) {
        snprintf(line, sizeof(line), "AMMO %d", player->weaponOnHand.ammoCount);
        drawText(line, w - pad - textWidth(line, 1.0f), pad, 1.0f, HUD_AMBER, 255);
    }
    if(player != NULL && MATCH_STATE == MATCH_RUNNING && role == SH_ROLE_HIDER && local->health > 0) {
        const char *status = player->exposed ? "EXPOSED" : "HIDDEN";
        drawText(status, w - pad - textWidth(status, 1.0f), pad, 1.0f,
                 player->exposed ? HUD_RED : HUD_CYAN, player->exposed ? (Uint8)(160 + 95 * pulse) : 230);
        drawBar(pad, h - pad - 18 * hudScale, 220 * hudScale, 14 * hudScale,
                local->health / 100.0f, (rgb_t){60, 230, 120});
        if(player->powerUntil > now) {
            float ratio = (player->powerUntil - now) / (float)SH_POWER_MS;
            drawText("RED HOT - TOUCH A HUNTER", pad, h - pad - 70 * hudScale, 0.8f, HUD_RED, 255);
            drawBar(pad, h - pad - 40 * hudScale, 220 * hudScale, 14 * hudScale, ratio, HUD_RED);
        }
        if(player->tags > 0) {
            snprintf(line, sizeof(line), "BURNS %d", player->tags);
            drawText(line, w - pad - textWidth(line, 0.8f), pad + 40 * hudScale, 0.8f, HUD_AMBER, 230);
        }
    }

    /* Centre banners. */
    float midY = h * 0.30f;
    if(MATCH_STATE == MATCH_WAITING) {
        long lobby = MAX(0, shGame.roundRemainingMs - (long)(now - shGame.lastUpdateAt));
        if(shGame.roundRemainingMs > 0) {
            snprintf(line, sizeof(line), "HUNT STARTS IN %ld", lobby / 1000 + 1);
            drawTextCentered(line, w * 0.5f, midY, 1.4f, HUD_AMBER, 255);
        } else {
            drawTextCentered("WAITING FOR PLAYERS", w * 0.5f, midY, 1.4f, HUD_WHITE, 255);
            drawTextCentered("the hunt starts with 2 or more", w * 0.5f, midY + 50 * hudScale, 0.8f, HUD_AMBER, 220);
        }
    } else if(MATCH_STATE == MATCH_RESULTS) {
        const char *result = shGame.winner == SH_WINNER_HUNTERS ? "HUNTERS WIN" :
                             shGame.winner == SH_WINNER_HIDERS ? "HIDERS WIN" : "ROUND OVER";
        rgb_t color = shGame.winner == SH_WINNER_HUNTERS ? HUD_CYAN : HUD_PINK;
        drawTextCentered(result, w * 0.5f, midY, 2.4f, color, 255);
        drawTextCentered("next round - roles rotate", w * 0.5f, midY + 80 * hudScale, 0.8f, HUD_WHITE, 220);
    } else if(MATCH_STATE == MATCH_RUNNING) {
        long release = MAX(0, shGame.releaseRemainingMs - (long)(now - shGame.lastUpdateAt));
        if(release > 0) {
            snprintf(line, sizeof(line), "HUNTERS RELEASED IN %ld", release / 1000 + 1);
            drawTextCentered(line, w * 0.5f, midY, 1.3f, HUD_AMBER, 255);
            drawTextCentered(role == SH_ROLE_HUNTER ?
                             "shoot the hiders. they only show in the light" :
                             role == SH_ROLE_HIDER ?
                             "stay in the shadows. red pellets let you burn hunters" :
                             "you join next round",
                             w * 0.5f, midY + 46 * hudScale, 0.75f, HUD_WHITE, 230);
        } else if(player != NULL && role == SH_ROLE_HUNTER && player->frozenUntil > now) {
            snprintf(line, sizeof(line), "BURNED! FROZEN %lu", (player->frozenUntil - now) / 1000 + 1);
            drawTextCentered(line, w * 0.5f, midY, 1.4f, HUD_ICE, 255);
        } else if(local != NULL && role == SH_ROLE_HIDER && local->health <= 0) {
            drawTextCentered("YOU WERE SHOT", w * 0.5f, midY, 1.6f, HUD_RED, 255);
            drawTextCentered("watching until the next round", w * 0.5f, midY + 56 * hudScale, 0.8f, HUD_WHITE, 220);
        }
    }

    if(MATCH_STATE != MATCH_RUNNING || shGame.releaseRemainingMs > 0)
        drawTextCentered("WASD move   MOUSE aim   CLICK shoot", w * 0.5f, h - pad - 30 * hudScale,
                         0.7f, HUD_WHITE, 200);
}

/**
 * Main render function.
 */
int renderSDL()
{
    int outputWidth = 0;
    int outputHeight = 0;

    if (SDL_GetCurrentRenderOutputSize(sdlRenderer, &outputWidth, &outputHeight) &&
        outputWidth > 0 && outputHeight > 0) {
        updatePlayerCamera(outputWidth, outputHeight);
        engineParameters.toWindowRatioX = (float)outputWidth / cameraRect.w;
        engineParameters.toWindowRatioY = (float)outputHeight / cameraRect.h;
        engineParameters.toWorldRatioX = cameraRect.w / (float)outputWidth;
        engineParameters.toWorldRatioY = cameraRect.h / (float)outputHeight;
        hudScale = outputHeight / 800.0f;
    }
    resolveSpriteIDs();

    /* Recoil: nudge the camera briefly when the local player's shot fires. */
    VectorEntity *shaker = stealth_localPlayer();
    if(shaker != NULL && SDL_getenv("SHADOWHUNT_OVERVIEW") == NULL) {
        unsigned long shakeNow = getTimeMillis();
        for(int i = 0; i < SH_MAX_SHOTS; i++) {
            sh_shot_t *shot = &shGame.shots[i];
            if(shot->time == 0 || shakeNow - shot->time > 70)
                continue;
            float dx = shot->x - shaker->pos.x, dy = shot->y - shaker->pos.y;
            if(dx * dx + dy * dy > 20 * 20)
                continue;
            float len = sqrtf((shot->endX - shot->x) * (shot->endX - shot->x) +
                              (shot->endY - shot->y) * (shot->endY - shot->y));
            if(len > 0.01f) {
                cameraRect.x -= (shot->endX - shot->x) / len * 1.5f;
                cameraRect.y -= (shot->endY - shot->y) / len * 1.5f;
            }
            break;
        }
    }

    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdlRenderer, 10, 38, 48, 255);
    SDL_RenderClear(sdlRenderer);

    float cameraWindow[4];
    cameraWindow[0] = cameraRect.x; cameraWindow[1] = cameraRect.y; cameraWindow[2] = cameraRect.w; cameraWindow[3] = cameraRect.h;
    for(int i = 0; i < TexRegHandle.texRegCount; i++)
    {
        textureRegion_t *texReg = &TexRegHandle.texRegList[i];
        if(checkRectIntersect(texReg->area, cameraWindow))
            renderWorldTexture(texReg);
    }

    /* Coloured lamp pools on the ground. */
    for(int i = 0; i < world.lightCount; i++) {
        rgb_t color = lampPalette[i % (int)(sizeof(lampPalette) / sizeof(lampPalette[0]))];
        drawGlow(world.lights[i].x, world.lights[i].y, world.lights[i].radius * 1.15f, color, 110);
    }

    VectorEntity *local = stealth_localPlayer();
    int role = stealth_localRole();

    /* Corpses first, then the living. */
    for(int pass = 0; pass < 2; pass++) {
        for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
            VectorEntity *vecEnt = &vectorEntityList[i];
            if(!actorVisible(vecEnt) || ((vecEnt->health <= 0) != (pass == 0)))
                continue;
            drawActor(vecEnt, 255);
        }
    }

    Uint8 darkness = MATCH_STATE != MATCH_RUNNING ? 150 :
                     role == SH_ROLE_HUNTER ? 238 : 192;
    renderDarkness(outputWidth, outputHeight, darkness);

    /* Things that shine through the dark. */
    drawPellets();
    unsigned long now = getTimeMillis();
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(!actorVisible(vecEnt))
            continue;
        PlayerData *player = stealth_player(vecEnt);
        if(player->powerUntil > now && vecEnt->health > 0) {
            float flicker = 0.8f + 0.2f * sinf(now * 0.03f);
            drawGlow(vecEnt->pos.x, vecEnt->pos.y, 22 * flicker, (rgb_t){255, 50, 20}, 220);
            drawActor(vecEnt, 255);
        } else if(player->frozenUntil > now && player->role == SH_ROLE_HUNTER) {
            drawRing(vecEnt->pos.x, vecEnt->pos.y, 9, HUD_ICE, 230);
            drawActor(vecEnt, 200);
        } else if(vecEnt == local) {
            /* You can always see yourself, as a shadow when hidden. */
            drawActor(vecEnt, player->role == SH_ROLE_HIDER && !player->exposed ? 150 : 255);
            drawRing(vecEnt->pos.x, vecEnt->pos.y, 8.5f,
                     player->role == SH_ROLE_HUNTER ? HUD_CYAN : HUD_PINK, 150);
        }
    }
    drawTracers();

    /* Pain flash when the local player is hit; shake once per hit or burn. */
    if(local != NULL) {
        PlayerData *localData = stealth_player(local);
        static unsigned long lastShakenHit;
        static bool wasFrozen;
        if(localData != NULL && localData->hitFlashUntil > now &&
           localData->hitFlashUntil != lastShakenHit) {
            lastShakenHit = localData->hitFlashUntil;
            addCameraShake(3.0f);
        }
        /* Burned: frozen after the round's release countdown has ended. */
        bool frozenNow = localData != NULL && localData->frozenUntil > now;
        if(frozenNow && !wasFrozen && shGame.releaseRemainingMs <= 0)
            addCameraShake(5.0f);
        wasFrozen = frozenNow;
        if(localData != NULL && localData->hitFlashUntil > now) {
            float strength = (localData->hitFlashUntil - now) / 180.0f;
            SDL_SetRenderDrawColor(sdlRenderer, 255, 20, 40, (Uint8)(110 * strength));
            SDL_RenderFillRect(sdlRenderer, NULL);
        }
    }

    renderHUD(outputWidth, outputHeight);

    const char *screenshotPath = SDL_getenv("SHADOWHUNT_SCREENSHOT_PATH");
    const char *screenshotDelay = SDL_getenv("SHADOWHUNT_SCREENSHOT_FRAME");
    int captureFrame = screenshotDelay != NULL && atoi(screenshotDelay) > 0 ? atoi(screenshotDelay) : 120;
    if(!screenshotSaved && screenshotPath != NULL && ++screenshotFrame >= captureFrame) {
        SDL_Surface *capture = SDL_RenderReadPixels(sdlRenderer, NULL);
        if(capture != NULL) {
            if(SDL_SaveBMP(capture, screenshotPath)) {
                printf("saved renderer screenshot: %s\n", screenshotPath);
                screenshotSaved = qtrue;
            } else {
                printf("failed to save renderer screenshot: %s\n", SDL_GetError());
            }
            SDL_DestroySurface(capture);
        }
    }

    SDL_RenderPresent(sdlRenderer);
    return 0;
}

/********************INITIALIZATION********************/

/**
 * Initializes the SDL graphics system
 * Replaces initGraphicsHandle from OpenGL version
 */
int initGraphicsHandleSDL(SDL_Renderer *renderer, int sx, int sy, int genzoneid, int isClient)
{
    sdlRenderer = renderer;
    
    // GraphicsHandle.swidth = sx;
    // GraphicsHandle.sheight = sy;
    // GraphicsHandle.genzoneid = genzoneid;
    // GraphicsHandle.cellsize = 10;

    // char *bmp_path;
    // SDL_asprintf(&bmp_path, "/Users/metalturtle/Documents/projects/c++/shadowhunt/build/res//boat_fishing_big.png");
    // printf("check path %s \n", bmp_path);
    // testTexture = loadTexture(bmp_path);
    
    // rect2xywh(&GraphicsHandle.camera.window, 220, 220,
    //          engineParameters.windowWidth / 10,
    //          engineParameters.windowHeight / 10);
    
    if (isClient)
    {
        printf("Initializing SDL rendering system...\n");
        printf("Screen size: %dx%d\n", sx, sy);
        
        // Create framebuffers
        createTextureBufferSDL();
        createLightFrameBufferSDL();
        
        // Initialize texture and sprite systems
        printf("before init textures sdl \n");
        initTexturesSDL(isClient);
        initSpritesSDL(isClient);
        
        // Load font
        SDL_Texture *fontTex = NULL;
        // loadTextureSDL("res//GUI//fonttest.png", &fontTexture, &fontTex);
        fontTex = loadTexture("res//GUI//fonttest.png");
        fontTextureSDL = fontTex;

        lightTexture = createLightTexture(false);
        glowTexture = createLightTexture(true);
        /* dst.rgb unchanged, dst.a *= (1 - src.a): cuts holes in the mask. */
        eraseBlendMode = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
        eraseSupported = lightTexture != NULL &&
                         SDL_SetTextureBlendMode(lightTexture, eraseBlendMode);
        printf("darkness mask: %s\n", eraseSupported ? "light holes enabled" : "flat fallback");
        
        printf("SDL rendering system initialized successfully\n");
    }

    // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    
    // printf("graphics handle sdl \n");
    return 0;
}

/**
 * Cleanup function to free SDL resources
 */
void cleanupSDLRenderer()
{
    destroySDLFrameBuffer(&frameBufferTexture);
    destroySDLFrameBuffer(&light1DBuffer);
    destroySDLFrameBuffer(&occluderBuffer);
    destroySDLFrameBuffer(&lightBuffer);
    
    if (fontTextureSDL)
    {
        SDL_DestroyTexture(fontTextureSDL);
        fontTextureSDL = NULL;
    }
    
    // Free sprite textures
    for (int i = 0; i < SpriteHandle.imgCount; i++)
    {
        SDL_Texture *tex = SpriteHandle.texImgList[i];
        if (tex)
        {
            SDL_DestroyTexture(tex);
        }
    }
    
    // Free animated sprite frame textures
    for (int i = 0; i < AnimSpriteHandle.imgCount; i++)
    {
        SDL_Texture **frameTextures = AnimSpriteHandle.animSpriteList[i].texImage;
        if (frameTextures)
        {
            int frameCount = (int)AnimSpriteHandle.animSpriteList[i].total;
            for (int j = 0; j < frameCount; j++)
            {
                if (frameTextures[j])
                {
                    SDL_DestroyTexture(frameTextures[j]);
                }
            }
            zidfree(frameTextures);
        }
    }
    
    printf("SDL renderer cleaned up\n");
}
