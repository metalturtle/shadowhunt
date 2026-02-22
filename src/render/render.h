#ifndef RENDER_H
#define RENDER_H

#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "../engine/entity.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#define VERTSIZE 32

// typedef struct textureImage_st
// {
//     unsigned char *data;
//     int width, height;
//     int colorType;
// } textureImage_t;

typedef struct textureRegion_st
{
    int texID;
    rect2_t area;
    // float vert[VERTSIZE];
    float xyList[8];
    float renderXYList[8];
    SDL_FColor colorList[4];
    float uvList[8];
    byte indexList[6];

} textureRegion_t;

// SDL_Texture *textureList

typedef struct textureImageHandle_st
{
    // unsigned int *texNameList;
    SDL_Texture **texImgList;
    int texImgCount;
} textureImageHandle_t;

typedef struct textureRegionHandle_st
{
    // int *texIDList;

    // unsigned int *VAOList;

    s2imap_t *texNameMap;

    textureRegion_t *texRegList;
    int texRegCount;
} textureRegionHandle_t;

typedef struct spriteHandle_st
{
    int spriteVAO;

    // unsigned int *texNameList;
    // s2imap_t *nameMap;
    SDL_Texture **texImgList;
    int imgCount;

} spriteHandle_t;

typedef struct animatedSpriteImage_st
{
    SDL_Texture **texImage;
    unsigned int texName;
    int row;
    int col;
    float total;

} animatedSpriteImage_t;

typedef struct animatedSpriteHandle_st
{
    int spriteVAO;

    animatedSpriteImage_t *animSpriteList;

    int imgCount;

} animatedSpriteHandle_t;

// typedef struct graphicsHandle_st
// {
//     int swidth;
//     int sheight;
//     int cellsize;

//     int genzoneid;

//     unsigned int vertexShader;
//     vector(unsigned int) shaderProgramList;
//     vector(unsigned int) VAOList;

//     camera_t camera;

// } graphicsHandle_t;

textureImageHandle_t TexImgHandle;
textureRegionHandle_t TexRegHandle;
spriteHandle_t SpriteHandle;
animatedSpriteHandle_t AnimSpriteHandle;
// graphicsHandle_t GraphicsHandle;

// extern camera_t worldCamera;
extern entitySpriteList_t entSpriteList;
extern animatedSpriteList_t animSpriteList;

extern int initGraphicsHandle(int swidth, int height, int zoneid, int isClient);
extern int render();

#endif