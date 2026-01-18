#ifndef DYNAMICSTRING_H
#define DYNAMICSTRING_H

#include <stddef.h>

typedef struct dystring{
    char *string;
    size_t max_size;
    size_t curr_size;
}dystring;

void init_dystring(dystring* ds);
void append_dystring(dystring* ds, char c);
void free_dystring(dystring *ds);

typedef struct dyarray
{
    char **str;
    int curr_size;
    int max_size;
} dyarray;

dyarray *new_dyarray();
void init_dyarray(dyarray *da);
void append_dyarray(dyarray *da, char *s);
void free_dyarray(dyarray *da);

#endif