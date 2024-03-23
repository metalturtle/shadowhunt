#include "../basic/world_def.h"
#include "cJSON/cJSON.h"
#include "render.h"
#include "glad/glad.c"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "../movement/movement.h"
#include "png_handle.h"

char infoLog[512];

float vertices[] = {
    0, 0, 0,    1, 1, 1,  0, 0,
    0, 10, 0,   1, 1, 1,  0, 1,
    10, 10, 0,  1, 1, 1,  1, 1,
    10, 0, 0,   1, 1, 1,  1, 0
};

float fontVertices[] = {
    0, 0, 0,    1, 1, 1,  0, 0,
    0, 1, 0,   1, 1, 1,  0, -1,
    1, 1, 0,  1, 1, 1,  1, -1,
    1, 0, 0,   1, 1, 1,  1, 0
    };

unsigned int indices[] = {
    0, 1, 2,
    0, 2, 3
};

int FB_TEXTURE_ID;
int FB_LIGHT1D_ID;
int FB_OCCLUDER_ID;
int FB_LIGHT_ID;

int colorTextureBuffer = 0;
int light1DBuffer;
int occluderBuffer;
int lightBuffer;

int screenVAO;

textureImage_t fontTexture;
unsigned int fontTextureID;
int fontVAO;

/********************INSERT OPENGL OBJECTS********************/

intPair_t createFrameBuffer(int framex, int framey)
{
    unsigned int framebuffer;
    intPair_t bufID;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // generate texture
    unsigned int textureColorBuffer;
    glGenTextures(1, &textureColorBuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framex, framey, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // attach it to currently bound framebuffer object
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorBuffer, 0);

    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 800, 600);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
    }
    // std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    bufID.a = framebuffer;
    bufID.b = textureColorBuffer;
    // return textureColorBuffer;
    return bufID;
}

void createTextureBuffer()
{
    intPair_t bufID;
    bufID = createFrameBuffer(getScreenWidth(), getScreenHeight());
    FB_TEXTURE_ID = bufID.a;
    colorTextureBuffer = bufID.b;
}

void createLightFrameBuffer()
{
    int lightSize = 512;
    int occluder = 256;
    intPair_t bufID;
    bufID = createFrameBuffer(lightSize, 1);
    FB_LIGHT1D_ID = bufID.a;
    light1DBuffer = bufID.b;

    bufID = createFrameBuffer(occluder, occluder);
    FB_OCCLUDER_ID = bufID.a;
    occluderBuffer = bufID.b;

    bufID = createFrameBuffer(getScreenWidth(), getScreenHeight());
    FB_LIGHT_ID = bufID.a;
    lightBuffer = bufID.b;
}

void loadTexture(const char *path, textureImage_t *texImg, unsigned int *texName)
{   
    printf("at loadTexture\n");
    int error = readPNG(path, texImg);
    printf("after readPNG\n");

    if (error)
    {
        printf("\nError reading %s \n", path);
        return;
    }

    glGenTextures(1,texName);
    printf("checking texname %d \n", *texName);
    glBindTexture(GL_TEXTURE_2D, *texName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    int colval = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, colval, texImg->width,
                 texImg->height, 0, colval, GL_UNSIGNED_BYTE, texImg->data);

}

void loadAnimTexture(const char *path, textureImage_t *texImg, int row, int col, unsigned int *texName)
{
    int error = readPNG(path, texImg);

    if(error)
    {
        printf("\nError reading %s\n", path);
    }

    glGenTextures(1, texName);
    glBindTexture(GL_TEXTURE_2D_ARRAY, *texName);

    float width = texImg->width/col;
    float height = texImg->height/row;


    printf("checking width and height %f %f \n", width, height);

    // width = 2;
    // height = 2;
    float layerCount = 1;
    float mipLevelCount = 1;

    // Read you texels here. In the current example, we have 2*2*2 = 8 texels, with each texel being 4 GLubytes.
    // GLubyte texels[32] = 
    // {
    //     // Texels for first image.
    //     0,   0,   0,   255,
    //     255, 0,   0,   255,
    //     0,   255, 0,   255, 
    //     0,   0,   255, 255,
    //     // Texels for second image.
    //     255, 255, 255, 255,
    //     255, 255,   0, 255,
    //     0,   255, 255, 255,
    //     255, 0,   255, 255,
    // };
    
    // width = 45;
    // height = 32;
    // layerCount = 8;

    float maxWidth = texImg->width;
    float maxHeight = texImg->height;
    layerCount = row * col;

    unsigned char *pngData = (unsigned char *) zidmalloc(GENERALZONE, (int)(width * height * layerCount * 4.0));

    unsigned char *curPngPtr = pngData;

    for(int r = 0; r < row; r++)
    {
        for(int c = 0; c < col; c++)
        {
            int hoffset = r * height;
            int woffset = c * width;
    
            for(int h = 0; h < (int)height; h++)
            {
                for(int w = 0; w < (int) width * 4; w++)
                {
                    curPngPtr[h * (int)width * 4 + w]
                        = texImg->data[((h + hoffset) * (int)maxWidth * 4 + w + woffset * 4)];
                }
            }
            if(c != col - 1)
                curPngPtr = (curPngPtr + (int)(width * height * 4.0));
        }
        if(r != row - 1)
            curPngPtr = (curPngPtr + (int)(width * height * 4.0));
    }

    // for(int i = 0; i < (int)(maxWidth * maxHeight * 4.0); i++)
    // {
    //     texImg->data[i] = pngData[i];
    // }

    // ((h + hoffset) * maxWidth + w + woffset)

    // woffset = col * width
    // hoffset = row * height
    
    // printf("checking max val: %d %d (%p, %p,%p)\n", counting, (int)(maxWidth * maxHeight * 4), pngData, curPngPtr, pngData + (int)(maxWidth * maxHeight * 4));

    // for(int l = 0; l < layerCount; l++)
    // {
    //     for(int i = 0; i < (int)height; i++)
    //     {
    //         for(int j = 0; j < (int)width * 4; j++)
    //         {
    //             curPngPtr[(i * (int)width) * 4 + j] = texImg->data[(i * ((int)maxWidth * 4)) + j];
    //             // printf("checking bytes %u \n", pngData[i * (int)width + j]);
    //         }
    //     }

    // }


    // for(int i = 0; i < (int)(width * height * 4); i++)
    // {
    //     pngData[i] = texImg->data[i];
    // }

    // int mipLevelCount = 1;
    // int layerCount = row * col;
    printf("layer row count: %f %f %f \n", 2.0, 2.0, layerCount);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevelCount, GL_RGBA8, width, height, layerCount);

    // glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 
    //          tileW, tileH, imageCount, 0,
    //          this->Image_Format, GL_UNSIGNED_BYTE, nullptr);

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, layerCount, GL_RGBA, GL_UNSIGNED_BYTE, 
    // texImg->data
    // texels
    pngData
    );

    // glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevelCount, GL_RGBA8, width, height, layerCount);
    // glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, layerCount, GL_RGBA, GL_UNSIGNED_BYTE, texels);


    // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    printf("created anim texture\n");
}

int createVertexShader(const char *fileName)
{
    const char *vertShaderString;
    unsigned int vert;
    int success;

    vertShaderString = getFileString(fileName, TEMPORARYZONE);
    if (!vertShaderString)
    {
        printf("vertex shader file not found \n");
        return 1;
    }
    vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertShaderString, NULL);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vert, 512, NULL, infoLog);
        printf("Failed to load vertex Shader: %s \n", infoLog);
        return 1;
    }

    GraphicsHandle.vertexShader = vert;
    zidfree((void *)vertShaderString);
    return 0;
}

void createFragmentShader(const char *fileName)
{
    unsigned int vertexShader, fragShader, shaderProgram;
    char *fragShaderString;
    int success;

    vertexShader = GraphicsHandle.vertexShader;

    fragShaderString = getFileString(fileName, TEMPORARYZONE);

    fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragShaderString, NULL);
    glCompileShader(fragShader);

    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
        printf("fragment shader failed to compile: %s\n", infoLog);
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("failed to create shader program: %s \n", infoLog);
    }

    glDeleteShader(fragShader);
    zidfree((void *)fragShaderString);
    vecpush(GraphicsHandle.shaderProgramList, unsigned int, shaderProgram);
}

int createVAO(float *vertices, unsigned int *indices, int vsize, int isize)
{
    unsigned int VBO, EBO, VAO;

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vsize, vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, isize, indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 8 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, 0, 8 * sizeof(float), (void *)(6 * sizeof(float)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    return VAO;
}

/********************LOAD FROM JSON FILE********************/

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

    printf("reading texture demand, total count %d\n", lines);

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
    return demandTexList;
}

void loadTextureAreas(cJSON *jsonTexAreas)
{
    int lines;
    cJSON *jsonTexAreaList, *jsonTexArea;
    textureRegion_t *texRegList;
    int* texIDList;
    unsigned int* VAOList;
    char *texName;
    float x, y, w, h;
    float u1, v1, u2, v2;
    int VAO;

    lines = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonTexAreas, "size"));
    jsonTexAreaList = cJSON_GetObjectItemCaseSensitive(jsonTexAreas, "object");
    texRegList = (textureRegion_t *)zidmalloc(GENERALZONE, sizeof(textureRegion_t) * lines );
    texIDList = (int *)zidmalloc(GENERALZONE, sizeof(int) * lines);
    VAOList = (unsigned int*) zidmalloc(GENERALZONE, sizeof(unsigned int) * lines);
   
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

        rect2xywh(texRegList[i].area, x, y, w, h);

        texRegList[i].vert[0 + voff] = x;
        texRegList[i].vert[1 + voff] = y;
        texRegList[i].vert[2 + voff] = 0;
        voff += 3;
        texRegList[i].vert[0 + voff] = 1;
        texRegList[i].vert[1 + voff] = 1;
        texRegList[i].vert[2 + voff] = 1;
        voff += 3;
        texRegList[i].vert[0 + voff] = u1;
        texRegList[i].vert[1 + voff] = v1;
        voff += 2;


        texRegList[i].vert[0 + voff] = x;
        texRegList[i].vert[1 + voff] = y + h;
        texRegList[i].vert[2 + voff] = 0;
        voff += 3;
        texRegList[i].vert[0 + voff] = 1;
        texRegList[i].vert[1 + voff] = 1;
        texRegList[i].vert[2 + voff] = 1;
        voff += 3;
        texRegList[i].vert[0 + voff] = u1;
        texRegList[i].vert[1 + voff] = v2;
        voff += 2;


        texRegList[i].vert[0 + voff] = x + w;
        texRegList[i].vert[1 + voff] = y + h;
        texRegList[i].vert[2 + voff] = 0;
        voff += 3;
        texRegList[i].vert[0 + voff] = 1;
        texRegList[i].vert[1 + voff] = 1;
        texRegList[i].vert[2 + voff] = 1;
        voff += 3;
        texRegList[i].vert[0 + voff] = u2;
        texRegList[i].vert[1 + voff] = v2;
        voff += 2;


        texRegList[i].vert[0 + voff] = x + w;
        texRegList[i].vert[1 + voff] = y;
        texRegList[i].vert[2 + voff] = 0;
        voff += 3;
        texRegList[i].vert[0 + voff] = 1;
        texRegList[i].vert[1 + voff] = 1;
        texRegList[i].vert[2 + voff] = 1;
        voff += 3;
        texRegList[i].vert[0 + voff] = u2;
        texRegList[i].vert[1 + voff] = v1;
        voff += 2;

        texIDList[i] = s2imap_get(TexRegHandle.texNameMap, texName);
        VAO = createVAO(texRegList[i].vert, indices, VERTSIZE * sizeof(float), sizeof(indices));
        VAOList[i] = VAO;
    }

    TexRegHandle.VAOList = VAOList;
    TexRegHandle.texIDList = texIDList;
    TexRegHandle.texRegList = texRegList;
    TexRegHandle.texRegCount = lines;
}

void initTextures(int isClient)
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
    }

    if(isClient)
    {
        glClearColor (0.2, 0.2, 0.8, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        TexImgHandle.texImgCount = texDemCount;
        TexImgHandle.texImgList = (textureImage_t *)zidmalloc(GENERALZONE
            , sizeof(textureImage_t) * texDemCount);

        TexImgHandle.texNameList = (unsigned int *) zidmalloc(GENERALZONE
            , sizeof(unsigned int) * texDemCount);

        for (int i = 0; i < texDemCount; i++)
        {
            strcpy(chartemp, levelFile);
            tempc = chartemp + strlen(levelFile);
            strcpy(tempc, demTexList[i]);
        
            loadTexture(chartemp, &TexImgHandle.texImgList[i], &TexImgHandle.texNameList[i]);
            // loadAnimTexture(chartemp, &TexImgHandle.texImgList[i], 1, 1, &TexImgHandle.texNameList[i]);
        }

        cJSON *jsonTexAreas = cJSON_GetObjectItemCaseSensitive(json, "texture");
        loadTextureAreas(jsonTexAreas);
    }
    
    cJSON_free(json);
    zidfree(fbuf);
}

extern void sprite_init();
extern void sprite_add(char *spriteName, int id);
extern int sprite_getID(char *spriteName);

void initSprites(int isClient)
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

    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);

    jsonSpriteFolder = cJSON_GetObjectItemCaseSensitive(jsonSpriteList, "folder");
    spriteFolder = cJSON_GetStringValue(jsonSpriteFolder);

    SpriteHandle.texImgList = (textureImage_t *) zidmalloc(PERMANENTZONE, sizeof(textureImage_t) * spriteLen);
    SpriteHandle.texNameList = (unsigned int *) zidmalloc(PERMANENTZONE, sizeof(unsigned int) * spriteLen);
    
    sprite_init();

    if(isClient)
    {
        SpriteHandle.spriteVAO = createVAO(vertices, indices, sizeof(vertices), sizeof(indices));
        AnimSpriteHandle.spriteVAO = SpriteHandle.spriteVAO;
    }

    jsonSpriteObj = NULL;

    cJSON_ArrayForEach(jsonSpriteObj, jsonSpriteList)
    {
        spriteObjName = jsonSpriteObj->string;
        spriteFileName = cJSON_GetStringValue(jsonSpriteObj);

        if(strcmp(spriteObjName, "folder") == 0)
            continue;

        writePath = spriteFilePath;
        strcpy(writePath, spriteFolder);

        writePath += strlen(spriteFolder);
        strcpy(writePath, pathSep);

        writePath += strlen(pathSep);
        strcpy(writePath, spriteFileName);

        printf("reading file: %s\n", spriteFilePath);
        
        if(isClient)
        {
            loadTexture(spriteFilePath, &SpriteHandle.texImgList[i], &SpriteHandle.texNameList[i]);
        }

        sprite_add(spriteObjName, i, SPRITE_TYPE_STATIC);

        i++;
    }

    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "animated_sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);

    jsonSpriteFolder = cJSON_GetObjectItemCaseSensitive(jsonSpriteList, "folder");
    spriteFolder = cJSON_GetStringValue(jsonSpriteFolder);

    AnimSpriteHandle.animSpriteList = (animatedSpriteImage_t *) zidmalloc(PERMANENTZONE, sizeof(animatedSpriteImage_t) * spriteLen);

    jsonSpriteObj = NULL;
    i=0;
    cJSON_ArrayForEach(jsonSpriteObj, jsonSpriteList)
    {
        spriteObjName = jsonSpriteObj->string;
        jsonAnimFileName = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "file");

        spriteFileName = cJSON_GetStringValue(jsonAnimFileName);

        printf("spriteObjName %s %s\n", spriteObjName, spriteFileName);

        if(strcmp(spriteObjName, "folder") == 0)
            continue;
        
        spriteObjName = jsonSpriteObj->string;
        jsonAnimFileName = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "file");

        writePath = spriteFilePath;
        strcpy(writePath, spriteFolder);

        writePath += strlen(spriteFolder);
        strcpy(writePath, pathSep);

        writePath += strlen(pathSep);
        strcpy(writePath, spriteFileName);

        printf("reading file: %s\n", spriteFilePath);
    
        jsonNumVal = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "row");
        row = cJSON_GetNumberValue(jsonNumVal);
        jsonNumVal = cJSON_GetObjectItemCaseSensitive(jsonSpriteObj, "col");
        col = cJSON_GetNumberValue(jsonNumVal);

        AnimSpriteHandle.animSpriteList[i].row = row;
        AnimSpriteHandle.animSpriteList[i].col = col;
        AnimSpriteHandle.animSpriteList[i].total = row * col;


        if(isClient)
        {
                // glTexImage2D(GL_TEXTURE_2D, 0, colval, texImg->width,
                //  texImg->height, 0, colval, GL_UNSIGNED_BYTE, texImg->data);

            // loadTexture(spriteFilePath, &SpriteHandle.texImgList[i], &SpriteHandle.texNameList[i]);
            // loadTexture(spriteFilePath, &AnimSpriteHandle.animSpriteList[i].texImage, &AnimSpriteHandle.animSpriteList[i].texName);
            // row = 1;
            // col = 2;
            printf("anim rowcol: %d %d \n", row, col);
            loadAnimTexture(spriteFilePath, &AnimSpriteHandle.animSpriteList[i].texImage, row, col, &AnimSpriteHandle.animSpriteList[i].texName);
        }

        sprite_add(spriteObjName, i, SPRITE_TYPE_ANIM);

        printf("\n\n");
        i++;
    }

    cJSON_free(json);
    zidfree(fbuf);
}

/********************RENDER********************/

void drawRect(int texregid)
{
    int shaderProgram = vecget(GraphicsHandle.shaderProgramList, 0);
    int VAO = TexRegHandle.VAOList[texregid];
    float swidth = GraphicsHandle.swidth;
    float sheight = GraphicsHandle.sheight;
    float cellsize = GraphicsHandle.cellsize;
    camera_t camera = GraphicsHandle.camera;
    int texid = TexRegHandle.texIDList[texregid];

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, swidth / cellsize, 0.0f, sheight / cellsize, -10.0f, 100.0f);

    trans = glm::translate(trans, glm::vec3(-camera.window[0], -camera.window[1], 0.0f));

    glUseProgram(shaderProgram);
    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexImgHandle.texNameList[texid]);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
}

void drawSprite(int spriteID, float x, float y, float rect[4], float angle)
{
    int shaderProgram = vecget(GraphicsHandle.shaderProgramList, 0);
    int VAO = SpriteHandle.spriteVAO;
    float swidth = GraphicsHandle.swidth;
    float sheight = GraphicsHandle.sheight;
    float cellsize = GraphicsHandle.cellsize;
    camera_t camera = GraphicsHandle.camera;
    unsigned int texName = SpriteHandle.texNameList[spriteID];

    rect[2] /= cellsize;
    rect[3] /= cellsize;

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, swidth / cellsize, 0.0f, sheight / cellsize, -10.0f, 100.0f);
    trans = glm::translate(trans, glm::vec3(-camera.window[0] + x, -camera.window[1] + y, 0.0f));
    trans = glm::rotate(trans, angle, glm::vec3(0.0f, 0.0f, 1.0f));
    trans = glm::translate(trans, glm::vec3(rect[0], rect[1], 0.0f));
    trans = glm::scale(trans, glm::vec3(rect[2], rect[3], 1.0f));

    glUseProgram(shaderProgram);

    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    // if(spriteID == 2)
    //     printf("rendering red sprite %d \n", texName);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texName);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
}

void drawAnimSprite(animatedSprite_t *entSprite)
{
    int shaderProgram = vecget(GraphicsHandle.shaderProgramList, 1);
    int VAO = AnimSpriteHandle.spriteVAO;
    float swidth = GraphicsHandle.swidth;
    float sheight = GraphicsHandle.sheight;
    float cellsize = GraphicsHandle.cellsize;
    camera_t camera = GraphicsHandle.camera;
    unsigned int texName;

    animatedSpriteImage_t *animSpriteImage;
    animSpriteImage = &AnimSpriteHandle.animSpriteList[entSprite->texID];
    texName = animSpriteImage->texName;

    float x = entSprite->pos[0];
    float y = entSprite->pos[1];
    float rx = entSprite->rect[0];
    float ry = entSprite->rect[1];
    float w = entSprite->rect[2]/cellsize;
    float h = entSprite->rect[3]/cellsize;
    float angle = entSprite->angle;

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, swidth / cellsize, 0.0f, sheight / cellsize, -10.0f, 100.0f);
    trans = glm::translate(trans, glm::vec3(-camera.window[0] + x, -camera.window[1] + y, 0.0f));
    trans = glm::rotate(trans, angle, glm::vec3(0.0f, 0.0f, 1.0f));
    trans = glm::translate(trans, glm::vec3(rx, ry, 0.0f)); 
    trans = glm::scale(trans, glm::vec3(w, h, 1.0f));
    

    // printf("anim texID: %d \n", entSprite->texID);
    // glUseProgram(shaderProgram);

    // unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    // glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));


    // printf("rendering anim sprite %d \n", texName);
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, texName);
    // glBindVertexArray(VAO);
    // glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);

    glUseProgram(shaderProgram);

    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    // float curLayer = entSprite->curSprite * animSpriteImage->total;
    float curLayer = entSprite->curSprite * 8;
    // printf("curLayer: %f %f %f \n", curLayer, animSpriteImage->total);
    glUniform1f(glGetUniformLocation(shaderProgram, "currentLayer"), curLayer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texName);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
}

void renderLight()
{
    int shadow1DShader = vecget(GraphicsHandle.shaderProgramList, 2);
    int lightShader = vecget(GraphicsHandle.shaderProgramList, 3);
    camera_t camera = GraphicsHandle.camera;
    float val = 256;
    rect2_t wall;
    rect2_t bound;
    float lineShaderHeight = 1;
    // int VAO = SpriteHandle.spriteVAO;
    int VAO = screenVAO;

    glBindFramebuffer(GL_FRAMEBUFFER, FB_LIGHT_ID); // back to default
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, FB_OCCLUDER_ID);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for(int i = 0; i < world.worldWallSize; i++)
    {
        rect2set(wall, world.worldWallArray[i].rect);

        bound[0] = bound[1] = 0;
        bound[2] = wall[2];
        bound[3] = wall[3];

        drawSprite(2, wall[0], wall[1], bound, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, FB_LIGHT1D_ID);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shadow1DShader);

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, val, 0.0f, lineShaderHeight, -10.0f, 100.0f);
    trans = glm::scale(trans, glm::vec3(val, lineShaderHeight, 1.0f));

    unsigned int transformLoc = glGetUniformLocation(shadow1DShader, "resolution");
    glUniform2f(transformLoc, val, val);

    transformLoc = glGetUniformLocation(shadow1DShader, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, occluderBuffer);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, FB_LIGHT_ID);
    trans = glm::ortho(0.0f, val, val, 0.0f, -10.0f, 100.0f);
    trans = glm::scale(trans, glm::vec3(val, val, 1.0f));
    glUseProgram(lightShader);

    transformLoc = glGetUniformLocation(lightShader, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    transformLoc = glGetUniformLocation(lightShader, "softShadows");
    glUniform1f(transformLoc, 1.0f);

    transformLoc = glGetUniformLocation(lightShader, "resolution");
    glUniform2f(transformLoc, 1, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, light1DBuffer);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void renderFont(char *letter, float x, float y)
{
    float swidth = GraphicsHandle.swidth;
    float sheight = GraphicsHandle.sheight;
    float cellsize = GraphicsHandle.cellsize;
    int VAO = SpriteHandle.spriteVAO;

    glm::mat4 trans = glm::mat4(1.0f);
    float aspect = ((float)fontTexture.height)/((float)fontTexture.width);

    //     trans = glm::ortho(0.0f, swidth / cellsize, 0.0f, sheight / cellsize, -10.0f, 100.0f);
    // trans = glm::translate(trans, glm::vec3(-camera.window[0] + x, -camera.window[1] + y, 0.0f));
    // trans = glm::rotate(trans, angle, glm::vec3(0.0f, 0.0f, 1.0f));
    // trans = glm::translate(trans, glm::vec3(rect[0], rect[1], 0.0f));
    // trans = glm::scale(trans, glm::vec3(rect[2], rect[3], 1.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTextureID);
    glBindVertexArray(fontVAO);

    for(int i = 0; i < 3; i++)
    {
        char ach = letter[i];

        trans = glm::ortho(0.0f, swidth , 0.0f, sheight , -10.0f, 100.0f);
        trans = glm::translate(trans, glm::vec3(40 * i, 0, 0.0f));
            // trans = glm::rotate(trans, angle, glm::vec3(0.0f, 0.0f, 1.0f));
            // trans = glm::translate(trans, glm::vec3(rect[0], rect[1], 0.0f));
        trans = glm::scale(trans, glm::vec3(40,40, 1.0f));

        int shaderProgram = vecget(GraphicsHandle.shaderProgramList, 0);
        glUseProgram(shaderProgram);
        unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));


        float asciiMap[256];
        for(int i = 'a'; i <= 'z'; i++)
        {
            asciiMap[i] = i - 'a' + 1;
        }

        float alphaX = 1.0/32.0;
        float alphaY = 1.0/3.0;

        float yCoord = alphaY * 0;
        float xCoord = alphaX  * asciiMap[ach];

        float newVert[] = {
        0, 0, 0,    1, 1, 1,  xCoord, -yCoord,
        0, 1, 0,   1, 1, 1,  xCoord, -yCoord - alphaY,
        1, 1, 0,  1, 1, 1,  xCoord + alphaX, -yCoord - alphaY,
        1, 0, 0,   1, 1, 1,  xCoord + alphaX, -yCoord
        };

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(newVert), newVert);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // glDrawArrays(GL_TRIANGLES, 0, 6);
        glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
    }

}

int render()
{
    float col = 0.1;
    entitySprite_t *entSprite;
    animatedSprite_t *animSprite;
    rect2_t bound;
    // int shaderProgram;

    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // renderLight();

    glBindFramebuffer(GL_FRAMEBUFFER, FB_TEXTURE_ID);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT); // we're not using the stencil buffer now
    // glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // glClearColor(col ,col , col, 1.0);
    // glClear(GL_COLOR_BUFFER_BIT);
    // glEnable(GL_TEXTURE_2D);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    rect2xy(GraphicsHandle.camera.window, worldCamera.window[0], worldCamera.window[1]);


    // shaderProgram = vecget(GraphicsHandle.shaderProgramList, 0);
    // glUseProgram(shaderProgram);

    // for(int i = 0; i < TexRegHandle.texRegCount; i++)
    // {
    //     if(checkRectIntersect(TexRegHandle.texRegList[i].area, GraphicsHandle.camera.window))
    //     {
    //         drawRect(i);
    //     }
    // }


    rect2_t wall;
    for(int i = 0; i < world.worldWallSize; i++)
    {
        rect2set(wall, world.worldWallArray[i].rect);

        bound[0] = bound[1] = 0;
        bound[2] = wall[2];
        bound[3] = wall[3];

        drawSprite(2, wall[0], wall[1], bound, 0);
    }


    for(int i = 0; i < vecsize(entSpriteList.renderList); i++)
    {
        entSprite = &vecget(entSpriteList.renderList, i);


        drawSprite(entSprite->texID,
                entSprite->pos[0], entSprite->pos[1],
                entSprite->rect,
                entSprite->angle
                );
    }
    
    // shaderProgram = vecget(GraphicsHandle.shaderProgramList, 1);
    // glUseProgram(shaderProgram);
    

    for(int i = 0; i < vecsize(animSpriteList.renderList); i++)
    {
        animSprite = &vecget(animSpriteList.renderList, i);

        drawAnimSprite(animSprite);
        // drawSprite(animSprite->texID,
        //         animSprite->pos[0], animSprite->pos[1],
        //         animSprite->rect,
        //         animSprite->angle
        //         );
    }

    
    float dir[3];
    float pos[3];
    for(int i = 0; i < vecsize(renderRayList.xList); i++)
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

        drawSprite(2,
                pos[0], pos[1],
                bound,
                angle
                );
    }

    
    // second pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // back to default
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT);
    
    // screenShader.use();  
    glBindVertexArray(screenVAO);

    float swidth = GraphicsHandle.swidth;
    float sheight = GraphicsHandle.sheight;
    float cellsize = GraphicsHandle.cellsize;
    int VAO = SpriteHandle.spriteVAO;

    int shaderProgram = vecget(GraphicsHandle.shaderProgramList, 0);
    glUseProgram(shaderProgram);

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, 10.0f, 0.0f, 10.0f, -10.0f, 100.0f);
    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTextureBuffer);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);

    trans = glm::ortho(0.0f, 20.0f, 0.0f, 20.0f, -10.0f, 100.0f);
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, occluderBuffer);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);

    char *str = "string";
    // for(int i = 0; i < strlen(str); i++)
    // {
    //     renderFont(str[i], i, 0);
    // }
    renderFont("str", 0, 0);
    
    return 0;
}

/********************INIT GRAPHICS HANDLE********************/

int initGraphicsHandle(int sx, int sy, int genzoneid, int isClient)
{
    GraphicsHandle.swidth = sx;
    GraphicsHandle.sheight = sy;
    GraphicsHandle.genzoneid = genzoneid;
    GraphicsHandle.cellsize = 10;

    rect2xywh(GraphicsHandle.camera.window, 220, 220,
     GraphicsHandle.swidth/10,
     GraphicsHandle.sheight/10);

    if(isClient)
    {
        vecinit(genzoneid, GraphicsHandle.shaderProgramList, unsigned int, 10);
        vecinit(genzoneid, GraphicsHandle.VAOList, unsigned int, 10);

        glViewport(0, 0, GraphicsHandle.swidth, GraphicsHandle.sheight);

        createVertexShader("shaders//vertex.glsl");
        createFragmentShader("shaders//fragment.glsl");
        createFragmentShader("shaders//texArrayFragment.glsl");
        createFragmentShader("shaders//shadowMap.glsl");
        createFragmentShader("shaders//shadowRender.glsl");

        createTextureBuffer();

        createLightFrameBuffer();

        float vertices[] = {
        0, 0, 0,    1, 1, 1,  0, 0,
        0, 1, 0,   1, 1, 1,  0, 1,
        1, 1, 0,  1, 1, 1,  1, 1,
        1, 0, 0,   1, 1, 1,  1, 0
    };


        screenVAO = createVAO(vertices, indices, VERTSIZE * sizeof(float), sizeof(indices));
    }

    initTextures(isClient);

    initSprites(isClient);

    if(isClient) {
        loadTexture("res//GUI//fonttest.png", &fontTexture, &fontTextureID);
        fontVAO = createVAO(fontVertices, indices, VERTSIZE * sizeof(float), sizeof(indices));
    }

    return 0;
}

float getScreenWidth()
{
    return GraphicsHandle.swidth;
}

float getScreenHeight()
{
    return GraphicsHandle.sheight;
}