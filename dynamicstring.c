#include "dynamicstring.h"
#include <string.h>
#include <stdlib.h>

void init_dystring(dystring* ds){
    ds->curr_size = 0;
    ds->max_size = 64;
    ds->strings = (char **) malloc(sizeof(char *) * ds->max_size);
}

void append_dystring(dystring* ds, char *str){
    if(ds->curr_size >= ds->max_size - 1){
        ds->max_size *= 2;
        ds->strings = (char **) realloc(ds->strings, sizeof(char *) * ds->max_size);
    }
    ds->strings[ds->curr_size++] = strdup(str);
    ds->strings[ds->curr_size] = NULL;
}

void free_dystring(dystring *ds){
    if(ds == NULL)
        return;

    for(size_t i=0; i<ds->curr_size; i++){
        if(ds->strings[i] != NULL){
            free(ds->strings[i]);
        }
    }

    if(ds->strings !=NULL){
        free(ds->strings);
        ds->strings = NULL;
    }

    ds->curr_size = 0;
    ds->max_size = 0;
}