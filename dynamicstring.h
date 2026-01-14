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

#endif