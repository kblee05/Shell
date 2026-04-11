#include "dynamicstring.h"
#include <string.h>
#include <stdlib.h>

#define INIT_MAX_SIZE 64

struct dystring *new_dystring()
{
	struct dystring *ds = malloc(sizeof(struct dystring));
	*ds = init_dystring();
	return ds;
}

struct dystring init_dystring()
{
	struct dystring ds = {
		.curr_size = 0,
		.max_size = INIT_MAX_SIZE,
		.string = malloc(sizeof(char) * INIT_MAX_SIZE)
	};
	ds.string[0] = '\0';
	
	return ds;
}

void append_dystring(struct dystring* ds, char c)
{
	if (ds->curr_size >= ds->max_size - 1) {
		ds->max_size *= 2;
		ds->string = (char *) realloc(ds->string, sizeof(char) * ds->max_size);
	}

	ds->string[ds->curr_size++] = c;
	ds->string[ds->curr_size] = '\0';
}

void free_dystring(struct dystring *ds)
{
	if(ds == NULL)
		return;

	free(ds->string);
	ds->string = NULL;
	ds->curr_size = 0;
	ds->max_size = INIT_MAX_SIZE;
}

void merge_dystring(struct dystring *ds, const char *target)
{
    	for(size_t i = 0; i < strlen(target); i++)
        	append_dystring(ds, target[i]);
}

struct dyarray *new_dyarray()
{
	struct dyarray *da = malloc(sizeof(struct dyarray));
	*da = init_dyarray(da);
	return da;
}

struct dyarray init_dyarray()
{
	struct dyarray da = {
		.curr_size = 0,
		.max_size = INIT_MAX_SIZE,
		.str = malloc(sizeof(char *) * INIT_MAX_SIZE)
	};
	da.str[0] = NULL;

	return da;
}

void append_dyarray(struct dyarray *da, const char *s)
{
	if (da->curr_size + 1 >= da->max_size) {
		da->max_size *= 2;
		da->str = realloc(da->str, sizeof(char *) * da->max_size);
	}

	da->str[da->curr_size++] = strdup(s);
	da->str[da->curr_size] = NULL;
}

void free_dyarray(struct dyarray *da)
{
	if (da == NULL)
		return;
	
	for(int i = 0; da->str[i]; i++)
		free(da->str[i]);
	free(da->str);

	da->curr_size = 0;
	da->max_size = INIT_MAX_SIZE;
	da->str = NULL;
}