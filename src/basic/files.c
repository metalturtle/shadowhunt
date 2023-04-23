#include <stdio.h>
#include <stdlib.h>
#include "../lib/cJSON/cJSON.h"
#include "basic.h"

char *getFileString(const char *filename, int zoneid)
{
    char *fstring, *freader;
    FILE *fileptr;
    int count, maxcount;
    fileptr = fopen(filename, "r");

    if (fileptr == NULL)
    {
        printf("couldnt open file %s\n", filename);
        return NULL;
    }

    fseek(fileptr, 0L, SEEK_END);
    long fsize = ftell(fileptr);
    rewind(fileptr);

    fstring = (char *)zidmalloc(zoneid, fsize + 1);
    count = 0;
    maxcount = fsize & (~31);
    freader = fstring;
    while (count < maxcount && fread(freader, 32, 1, fileptr))
    {
        count += 32;
        freader += 32;
    }
    if (maxcount < fsize)
    {
        fread(freader, fsize - maxcount, 1, fileptr);
    }
    
    fstring[fsize] = '\0';
    
    fclose(fileptr);

    return fstring;
}

/********************LOADER********************/

char *levelFileString;
cJSON *levelJSON;

void openLevelFile()
{
    levelFileString = getFileString("levels//config//sprite.json", TEMPORARYZONE);
    levelJSON = cJSON_Parse(levelFileString);
}

char *getLevelFileString()
{
    return levelFileString;
}

cJSON *getLevelJSON()
{
    return levelJSON;
}

void closeLevelFile()
{
    cJSON_free(levelJSON);
    zidfree(levelFileString);
}