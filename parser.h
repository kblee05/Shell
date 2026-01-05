#ifndef PARSER_H
#define PARSER_H

#include "dynamicstring.h"

typedef enum{
    SEP_SYNC,  // ;
    SEP_ASYNC, // &
    SEP_NONE
} SepType;

typedef struct SepNode{
    char **args;
    SepType type;
    struct SepNode *next;
} SepNode;

SepNode *parse_sep(char **args);
void free_sep_list(SepNode *head);

typedef enum{
    LOGIC_AND,
    LOGIC_OR,
    LOGIC_NONE
} LogicType;

typedef struct LogicNode{
    char **args;
    LogicType type;
    struct LogicNode *next;
} LogicNode;

LogicNode *parse_logic(char **args);
void free_logic_list(LogicNode *head);

typedef struct PipeNode{
    char **args;
    struct PipeNode *next;
} PipeNode;

PipeNode *parse_pipe(char **args);
void free_pipe_list(PipeNode *head);

#endif