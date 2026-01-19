#include "dynamicstring.h"
#include <string.h>
#include <stdlib.h>

dystring *
new_dystring()
{
    dystring *ds = malloc(sizeof(dystring));
    init_dystring(ds);
    return ds;
}

void init_dystring(dystring* ds){
    ds->curr_size = 0;
    ds->max_size = 64;
    ds->string = (char *) malloc(sizeof(char) * ds->max_size);
}

void append_dystring(dystring* ds, char c){
    if(ds->curr_size >= ds->max_size - 1){
        ds->max_size *= 2;
        ds->string = (char *) realloc(ds->string, sizeof(char) * ds->max_size);
    }
    ds->string[ds->curr_size++] = c;
    ds->string[ds->curr_size] = '\0';
}

void free_dystring(dystring *ds){
    if(ds == NULL)
        return;

    free(ds->string);
    ds->string = NULL;
    ds->curr_size = 0;
    ds->max_size = 0;
}

void
merge_dystring(dystring *ds, char *target)
{
    size_t i;

    for(i = 0; i < strlen(target); i++)
        append_dystring(ds, target[i]);
}

dyarray *
new_dyarray()
{
    dyarray *da = malloc(sizeof(dyarray));
    init_dyarray(da);
    return da;
}

void
init_dyarray(dyarray *da)
{
    da->curr_size = 0;
    da->max_size = 128;
    da->str = malloc(sizeof(char *) * da->max_size);
    da->str[0] = NULL;
}

void
append_dyarray(dyarray *da, char *s)
{
    if(da->curr_size + 1 >= da->max_size)
    {
        da->max_size *= 2;
        da->str = realloc(da->str, sizeof(char *) * da->max_size);
    }

    da->str[da->curr_size++] = strdup(s);
    da->str[da->curr_size] = NULL;
}

void
free_dyarray(dyarray *da)
{
    for(int i = 0; da->str[i]; i++)
        free(da->str[i]);
    free(da->str);
}