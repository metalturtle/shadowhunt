#include "basic.h"
#include <ctype.h>

#define CVARHASHSIZE 1
#define MAXCVARS 1024

static	cvar_t* hashTable[CVARHASHSIZE];
static cvar_t cvarList[MAXCVARS];
static int cvarCount;


static long getHash( const char *fname )
{
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (CVARHASHSIZE-1);
	return hash;
}

static cvar_t *cvar_find( const char *name )
{
	cvar_t	*var;
	long hash;

	hash = getHash(name);
	for (var = hashTable[hash]; var; var = var->hashNext)
    {
		if (!strcmp(name, var->name)) {
			return var;
		}
	}
    
	return NULL;
}

static void addToHashTable(cvar_t *cv)
{
    int hash;
    hash = getHash(cv->string);
    if(hashTable[hash])
    {
        cv->hashNext = hashTable[hash];
        hashTable[hash] = cv;
        return;
    }
    cv->hashNext = NULL;
    hashTable[hash] = cv;
}

char *cvar_getString(const char *name)
{
    cvar_t *cv;
    cv = cvar_find(name);
    if(cv == NULL)
        return "";
    return cv->string;
}

int cvar_getInt(const char *name)
{
    cvar_t *cv;
    cv = cvar_find(name);
    if(cv == NULL)
        return 0;
    return cv->intval;
}

int cvar_getFloat(const char *name)
{
    cvar_t *cv;
    cv = cvar_find(name);
    if(cv == NULL)
        return 0;
    return cv->floatval;
}

void cvar_set2(cvar_t *cv, char *value)
{
    if(cv->string)
    {
        zidfree(cv->string);
    }
    cv->string = copyString(value, strlen(value) + 1);
    cv->floatval = atof(cv->string);
    cv->intval = atoi(cv->string);
}

cvar_t *cvar_get(const char *name, char *value)
{
    cvar_t *cv;

    if ( !name || ! value ) {
        com_error( ERR_FATAL, "cvar_get: NULL parameter" );
    }

    cv = cvar_find(name);
    if(cv)
    {
        cvar_set2(cv, value);
        return cv;
    }

    cv = &cvarList[cvarCount];
    cvarCount++;
    cv->name = copyString((char*) name, strlen(name) + 1);
    cvar_set2(cv, value);
    addToHashTable(cv);
    return cv;
}

void cvar_init(void)
{
    for(int i = 0; i < CVARHASHSIZE; i++)
    {
        hashTable[i] = NULL;
    }
    
    for(int i = 0; i < MAXCVARS; i++)
    {
        cvarList[i].string = NULL;
        cvarList[i].intval = cvarList[i].floatval = 0;
    }

    cvarCount = 0;
}
