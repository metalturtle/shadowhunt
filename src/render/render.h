#ifndef RENDER_H
#define RENDER_H

#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "../engine/engine.h"
#include "../engine/entity.h"

#define VERTSIZE 32

typedef struct textureImage_st
{
    unsigned char *data;
    int width, height;
    int colorType;
} textureImage_t;

typedef struct textureRegion_st
{
    rect2_t area;
    float vert[VERTSIZE];
} textureRegion_t;

typedef struct textureImageHandle_st
{
    unsigned int *texNameList;
    textureImage_t *texImgList;
    int texImgCount;
} textureImageHandle_t;

typedef struct textureRegionHandle_st
{
    int *texIDList;

    unsigned int *VAOList;

    s2imap_t *texNameMap;

    textureRegion_t *texRegList;
    int texRegCount;
} textureRegionHandle_t;

typedef struct spriteHandle_st
{
    int spriteVAO;

    unsigned int *texNameList;
    textureImage_t *texImgList;
    int imgCount;

} spriteHandle_t;

typedef struct animatedSpriteImage_st
{
    textureImage_t texImage;
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

typedef struct graphicsHandle_st
{
    int swidth;
    int sheight;
    int cellsize;

    int genzoneid;

    unsigned int vertexShader;
    vector(unsigned int) shaderProgramList;
    vector(unsigned int) VAOList;

    camera_t camera;

} graphicsHandle_t;

textureImageHandle_t TexImgHandle;
textureRegionHandle_t TexRegHandle;
spriteHandle_t SpriteHandle;
animatedSpriteHandle_t AnimSpriteHandle;
graphicsHandle_t GraphicsHandle;

extern camera_t worldCamera;
extern entitySpriteList_t entSpriteList;
extern animatedSpriteList_t animSpriteList;

extern int initGraphicsHandle(int swidth, int height, int zoneid, int isClient);
extern int render();

#endif