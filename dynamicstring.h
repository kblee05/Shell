#ifndef DYNAMICSTRING_H
#define DYNAMICSTRING_H

#include <stddef.h>

struct dystring {
    char *string;
    size_t max_size;
    size_t curr_size;
};

struct dystring *new_dystring();
struct dystring init_dystring();
void append_dystring(struct dystring* ds, char c);
void free_dystring(struct dystring *ds);
void merge_dystring(struct dystring *ds, const char *target);

struct dyarray {
    char **str;
    int curr_size;
    int max_size;
};

struct dyarray *new_dyarray();
struct dyarray init_dyarray();
void append_dyarray(struct dyarray *da, const char *s);
void free_dyarray(struct dyarray *da);

#endif