#include "../../basic/world_def.h"
#include "render.h"
#include "../../lib/cJSON/cJSON.c"
#include "png_handle.h"
#include "../lib/glad/glad.c"
#include "../../basic/files.h"
#include "../lib/glm/glm.hpp"
#include "../lib/glm/gtc/matrix_transform.hpp"
#include "../lib/glm/gtc/type_ptr.hpp"

// camera_t worldCamera;

char infoLog[512];

float vertices[] = {
    0, 0, 0,    1, 1, 1,  0, 0,
    0, 10, 0,   1, 1, 1,  0, 1,
    10, 10, 0,  1, 1, 1,  1, 1,
    10, 0, 0,   1, 1, 1,  1, 0
};

unsigned int indices[] = {
    0, 1, 2,
    0, 2, 3
};

/********************INSERT OPENGL OBJECTS********************/

void loadTexture(const char *path, textureImage_t *texImg, unsigned int *texName)
{   
    int error = readPNG(path, texImg);

    if (error)
    {
        printf("\nError reading %s \n", path);
        return;
    }

    glGenTextures(1,texName);
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

    int mipMapLevelCount = 1;
    int layerCount = row * col;
    // int layerCount = 2;
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipMapLevelCount, GL_RGBA8, width, height, layerCount);

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, layerCount, GL_RGBA, GL_UNSIGNED_BYTE, texImg->data);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

void initTextures()
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

    for (int i = 0; i < texDemCount; i++)
    {
        getTexName(demTexList[i], chartemp);
        s2imap_put(TexRegHandle.texNameMap, chartemp, i);
    }

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
    }

    cJSON *jsonTexAreas = cJSON_GetObjectItemCaseSensitive(json, "texture");
    loadTextureAreas(jsonTexAreas);

    cJSON_free(json);
    zidfree(fbuf);
}

extern void sprite_init();
extern void sprite_add(char *spriteName, int id);
extern int sprite_getID(char *spriteName);

void initSprites()
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

    SpriteHandle.spriteVAO = createVAO(vertices, indices, sizeof(vertices), sizeof(indices));

    fbuf = getFileString("levels//config//sprite.json", TEMPORARYZONE);

    cJSON *json = cJSON_Parse(fbuf);

    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);

    jsonSpriteFolder = cJSON_GetObjectItemCaseSensitive(jsonSpriteList, "folder");
    spriteFolder = cJSON_GetStringValue(jsonSpriteFolder);

    SpriteHandle.texImgList = (textureImage_t *) zidmalloc(PERMANENTZONE, sizeof(textureImage_t) * spriteLen);
    SpriteHandle.texNameList = (unsigned int *) zidmalloc(PERMANENTZONE, sizeof(unsigned int) * spriteLen);
    
    sprite_init();

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
        
        loadTexture(spriteFilePath, &SpriteHandle.texImgList[i], &SpriteHandle.texNameList[i]);

        sprite_add(spriteObjName, i, SPRITE_TYPE_STATIC);

        i++;
    }

    AnimSpriteHandle.spriteVAO = SpriteHandle.spriteVAO;

    jsonSpriteList = cJSON_GetObjectItemCaseSensitive(json, "animated_sprite");
    spriteLen = cJSON_GetArraySize(jsonSpriteList);
    printf("anim spriteLen %d\n", spriteLen);

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

        // loadTexture(spriteFilePath, &AnimSpriteHandle.animSpriteList[i].texImage, &AnimSpriteHandle.animSpriteList[i].texName);
        loadAnimTexture(spriteFilePath, &AnimSpriteHandle.animSpriteList[i].texImage, row, col, &AnimSpriteHandle.animSpriteList[i].texName);

        sprite_add(spriteObjName, i, SPRITE_TYPE_ANIM);

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

    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexImgHandle.texNameList[texid]);
    glBindVertexArray(VAO);
    glUseProgram(shaderProgram);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
}

void drawSprite(int spriteID, float x, float y, float w, float h)
{
    int shaderProgram = vecget(GraphicsHandle.shaderProgramList, 0);
    int VAO = SpriteHandle.spriteVAO;
    float swidth = GraphicsHandle.swidth;
    float sheight = GraphicsHandle.sheight;
    float cellsize = GraphicsHandle.cellsize;
    camera_t camera = GraphicsHandle.camera;
    unsigned int texName = SpriteHandle.texNameList[spriteID];

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, swidth / cellsize, 0.0f, sheight / cellsize, -10.0f, 100.0f);

    trans = glm::translate(trans, glm::vec3(-camera.window[0] + x, -camera.window[1] + y, 0.0f));

    w /= cellsize;
    h /= cellsize;
    trans = glm::scale(trans, glm::vec3(w, h, 1.0f));
    
    trans = glm::rotate(trans, 20.0f, glm::vec3(0.0f, 0.0f, 1.0f));

    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texName);
    glBindVertexArray(VAO);
    glUseProgram(shaderProgram);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
}

void drawAnimSprite(entitySprite_t *entSprite, animatedSprite_t *animSprite)
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

    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::ortho(0.0f, swidth / cellsize, 0.0f, sheight / cellsize, -10.0f, 100.0f);

    float x = entSprite->pos[0];
    float y = entSprite->pos[1];
    float w = entSprite->rect[2];
    float h = entSprite->rect[3];

    trans = glm::translate(trans, glm::vec3(-camera.window[0] + x, -camera.window[1] + y, 0.0f));

    w /= cellsize;
    h /= cellsize;
    trans = glm::scale(trans, glm::vec3(w, h, 1.0f));
    
    trans = glm::rotate(trans, 20.0f, glm::vec3(0.0f, 0.0f, 1.0f));

    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    float curLayer = animSprite->curSprite*animSpriteImage->total;

    glUniform1f(glGetUniformLocation(shaderProgram, "currentLayer"), curLayer);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texName);
    glBindTexture(GL_TEXTURE_2D, texName);
    glBindVertexArray(VAO);
    glUseProgram(shaderProgram);
    glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_INT, 0);
}

int render()
{
    float col = 0.1f;
    animatedSprite_t *animSprite;

    glClearColor(col ,col , col, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    rect2xy(GraphicsHandle.camera.window, worldCamera.window[0], worldCamera.window[1]);
    for(int i = 0; i < TexRegHandle.texRegCount; i++)
    {
        if(checkRectIntersect(TexRegHandle.texRegList[i].area, GraphicsHandle.camera.window))
        {
            drawRect(i);
        }
    }

    int k = 0;
    for(int i = 0; i < vecsize(entSpriteList.entSprite); i++)
    {
        entitySprite_t *entSprite;
        entSprite = &vecget(entSpriteList.entSprite, i);

        if(entSprite->type == SPRITE_TYPE_STATIC)
        {
            drawSprite(entSprite->texID,
                entSprite->pos[0], entSprite->pos[1],
                entSprite->rect[2], entSprite->rect[3]);
        }
        if(entSprite->type == SPRITE_TYPE_ANIM)
        {
            animSprite = &vecget(animSpriteList.animSprite, k++);
            drawAnimSprite(entSprite, animSprite);
        }
    }

    return 0;
}

/********************INIT GRAPHICS HANDLE********************/

int initGraphicsHandle(int sx, int sy, int genzoneid)
{
    GraphicsHandle.swidth = sx;
    GraphicsHandle.sheight = sy;
    GraphicsHandle.genzoneid = genzoneid;
    GraphicsHandle.cellsize = 10;
    vecinit(genzoneid, GraphicsHandle.shaderProgramList, unsigned int, 10);
    vecinit(genzoneid, GraphicsHandle.VAOList, unsigned int, 10);

    rect2xywh(GraphicsHandle.camera.window, 220, 220,
     GraphicsHandle.swidth/10,
     GraphicsHandle.sheight/10);

    TexImgHandle.texImgCount = 0;

    TexRegHandle.texNameMap = s2imap_create(GENERALZONE);
    glViewport(0, 0, GraphicsHandle.swidth, GraphicsHandle.sheight);

    createVertexShader("shaders//vertex.glsl");
    createFragmentShader("shaders//fragment.glsl");
    createFragmentShader("shaders//texArrayFragment.glsl");

    initTextures();

    initSprites();

    return 0;
}