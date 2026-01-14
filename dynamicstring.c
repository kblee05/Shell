#include "dynamicstring.h"
#include <string.h>
#include <stdlib.h>

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