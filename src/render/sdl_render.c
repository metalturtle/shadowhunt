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

static SDL_Texture* fontTexture;
static SDL_Texture *fontTextureSDL = NULL;

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
 SDL_Texture* loadTexture(char *bmp_path) {
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;
    char *loadPath;
    //sboat_small_vertical
    SDL_asprintf(&loadPath, "%s%s", SDL_GetBasePath(), bmp_path);
    printf("loadTexture %s \n", loadPath);
    // surface = SDL_Load(bmp_path);
    surface = IMG_Load(bmp_path);

    if(!surface) {
        SDL_Log("Couldn't load bitmap %s", SDL_GetError());
        return NULL;
    }
    SDL_free(loadPath);

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
    
    // Use IMG_Load directly - much more efficient
    SDL_Surface *atlasSurface = IMG_Load(path);
    
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
            if (SDL_BlitSurface(atlasSurface, &srcRect, frameSurface, NULL) != 0)
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

        // texRegList[i].vert[0 + voff] = x;
        // texRegList[i].vert[1 + voff] = y;
        // texRegList[i].vert[2 + voff] = 0;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = 1;
        // texRegList[i].vert[1 + voff] = 1;
        // texRegList[i].vert[2 + voff] = 1;
        // voff += 3;
        // texRegList[i].vert[0 + voff] = u1;
        // texRegList[i].vert[1 + voff] = v1;
        // voff += 2;


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
    
    fbuf = getFileString("levels//level.json", TEMPORARYZONE);
    
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

extern void sprite_init();
extern void sprite_add(char *spriteName, int id, int type);
extern int sprite_getID(char *spriteName, int type);

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
    
    fbuf = getFileString("levels//config//sprite.json", TEMPORARYZONE);
    
    cJSON *json = cJSON_Parse(fbuf);
    
    // cJSON *jsonTexAreas = cJSON_GetObjectItemCaseSensitive(json, "texture");
    // int lines = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonTexAreas, "size"));
    // printf("checking texture lines %d \n", lines);

    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);
    printf("spriteLen %d \n", spriteLen);
    
    jsonSpriteFolder = cJSON_GetObjectItemCaseSensitive(jsonSpriteList, "folder");
    spriteFolder = cJSON_GetStringValue(jsonSpriteFolder);
    
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
        row = cJSON_GetNumberValue(jsonNumVal);
        jsonNumVal = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "col");
        col = cJSON_GetNumberValue(jsonNumVal);
        
        AnimSpriteHandle.animSpriteList[i].row = row;
        AnimSpriteHandle.animSpriteList[i].col = col;
        AnimSpriteHandle.animSpriteList[i].total = row * col;
        
        // if (isClient)
        // {
        //     SDL_Texture **frameTextures = NULL;
        //     int frameCount = 0;
            
        //     loadAnimTextureSDL(spriteFilePath,
        //                      &AnimSpriteHandle.animSpriteList[i].texImage,
        //                      row, col,
        //                      &frameTextures,
        //                      &frameCount);
            
        //     // Store the texture array pointer (reinterpret texName as pointer)
        //     AnimSpriteHandle.animSpriteList[i].texName = (unsigned int)(uintptr_t)frameTextures;
        // }
        
        sprite_add(spriteObjName, i, SPRITE_TYPE_ANIM);
        
        i++;
    }
    
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
    printf("destRect %f %f %f %f\n", destRect.x, destRect.y, destRect.w, destRect.h);
    // Rotation center (relative to destination rectangle)
    // printf("checking size %d %d\n", texture->w, texture->h);

    SDL_FPoint center;
    center.x = 0;
    center.y = 0;
    // center.x = destRect.w / 2.0f;
    // center.y = destRect.h / 2.0f;
    
    // Convert angle from radians to degrees
    // float angleDegrees = radiansToDegrees(angle);
    float angleDegrees = 0;
    
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
    SDL_Texture **frameTextures = (SDL_Texture **)(uintptr_t)animSpriteImage->texName;
    
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
    destRect.x = screenX + entSprite->rect[0];
    destRect.y = screenY + entSprite->rect[1];
    destRect.w = entSprite->rect[2];
    destRect.h = entSprite->rect[3];
    
    // Rotation center
    SDL_FPoint center;
    center.x = entSprite->rect[2] / 2.0f;
    center.y = entSprite->rect[3] / 2.0f;
    
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
    
    float charWidth = 40.0f;
    float charHeight = 40.0f;
    
    // ASCII mapping for font atlas
    float alphaX = 1.0f / 32.0f;
    float alphaY = 1.0f / 3.0f;
    
    for (int i = 0; text[i] != '\0'; i++)
    {
        char ch = text[i];
        
        if (ch < 'a' || ch > 'z')
            continue;
        
        int charIndex = ch - 'a' + 1;
        
        // Calculate source rectangle in font atlas
        SDL_FRect srcRect;
        srcRect.x = alphaX * charIndex * fontTexture->w;
        srcRect.y = 0;
        srcRect.w = alphaX * fontTexture->w;
        srcRect.h = alphaY * fontTexture->h;
        
        // Calculate destination rectangle
        SDL_FRect destRect;
        destRect.x = x + (charWidth * i);
        destRect.y = y;
        destRect.w = charWidth;
        destRect.h = charHeight;
        
        SDL_RenderTexture(sdlRenderer, fontTextureSDL, &srcRect, &destRect);
    }
}

/**
 * Main render function - SDL version
 * Replaces the OpenGL render() function
 */
int renderSDL()
{
    entitySprite_t *entSprite;
    animatedSprite_t *animSprite;
    rect2_t bound;
    
    // Render to main framebuffer
    // bindSDLFrameBuffer(&frameBufferTexture);
    
    SDL_SetRenderDrawColor(sdlRenderer, 26, 26, 26, 255);
    SDL_RenderClear(sdlRenderer);
    
    // Update camera from world camera
    // rect2xy(GraphicsHandle.camera.window, cameraRect.x, cameraRect.y);
    
    float cameraWindow[4];
    cameraWindow[0] = cameraRect.x; cameraWindow[1] = cameraRect.y; cameraWindow[2] = cameraRect.w; cameraWindow[3] = cameraRect.h;
    printf("\n\n\n");
    for(int i = 0; i < TexRegHandle.texRegCount; i++)
    {
        textureRegion_t *texReg = &TexRegHandle.texRegList[i];
        if(checkRectIntersect(texReg->area, cameraWindow))
        {

            // drawRect(i);
            drawSpriteSDL(TexImgHandle.texImgList[texReg->texID], texReg->area[0], texReg->area[1], texReg->area, 0);
        }
    }

    // Render walls
    rect2_t wall;
    for (int i = 0; i < world.worldWallSize; i++)
    {
        rect2set(wall, world.worldWallArray[i].rect);
        
        bound[0] = bound[1] = 0;
        bound[2] = wall[2];
        bound[3] = wall[3];
        
        // drawSpriteSDL(TexImgHandle.texImgList[0], wall[0], wall[1], bound, 0);
        break;
    }

    // Render entity sprites
    for (int i = 0; i < vecsize(entSpriteList.renderList); i++)
    {
        entSprite = &vecget(entSpriteList.renderList, i);
        
        // drawSpriteSDL(entSprite->texID,
        //              entSprite->pos[0], entSprite->pos[1],
        //              entSprite->rect,
        //              entSprite->angle);
    }
    
    // Render animated sprites
    for (int i = 0; i < vecsize(animSpriteList.renderList); i++)
    {
        animSprite = &vecget(animSpriteList.renderList, i);
        // drawAnimSpriteSDL(animSprite);
    }
    
    // Render debug rays
    float dir[3];
    float pos[3];
    for (int i = 0; i < vecsize(renderRayList.xList); i++)
    {
        dir[0] = vecget(renderRayList.xDirList, i);
        dir[1] = vecget(renderRayList.yDirList, i);
        dir[2] = 0;
        pos[0] = vecget(renderRayList.xList, i);
        pos[1] = vecget(renderRayList.yList, i);
        pos[2] = 0;
        
        float angle = vec3getang2(dir);
        float dist = vec3length(dir);
        
        bound[0] = bound[1] = 0;
        bound[2] = dist;
        bound[3] = 1;
        
        // drawSpriteSDL(2, pos[0], pos[1], bound, angle);
    }
    
    // Render framebuffer to screen
    // bindSDLFrameBuffer(NULL); // Render to screen
    // SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    // SDL_RenderClear(sdlRenderer);
    
    // printf("render texture \n");
    // Draw the main framebuffer texture to screen
    // SDL_RenderTexture(sdlRenderer, frameBufferTexture.texture, NULL, NULL);
    
    // printf("render font sdl \n");
    // Render font overlay
    // renderFontSDL("test", 40, 40);
    
    // printf("render present \n");

    // Present the rendered frame
    SDL_RenderPresent(sdlRenderer);

    // printf("done render \n");
    
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

    char *bmp_path;
    SDL_asprintf(&bmp_path, "/Users/metalturtle/Documents/projects/c++/shadowhunt/build/res//boat_fishing_big.png");
    printf("check path %s \n", bmp_path);
    testTexture = loadTexture(bmp_path);
    
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
        
        printf("SDL rendering system initialized successfully\n");
    }
    
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
        SDL_Texture **frameTextures = (SDL_Texture **)(uintptr_t)AnimSpriteHandle.animSpriteList[i].texName;
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