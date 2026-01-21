#ifndef PARSER_H
#define PARSER_H

#include "dynamicstring.h"
#include "jobcontrol.h"

typedef struct SepNode{
    char *cmd;
    char sync;
    struct SepNode *next;
} SepNode;

SepNode *parse_sep(char *str);
void free_sep_list(SepNode *head);

typedef enum{
    LOGIC_AND,
    LOGIC_OR,
    LOGIC_NONE
} LogicType;

typedef struct LogicNode{
    char *cmd;
    LogicType type;
    struct LogicNode *next;
} LogicNode;

LogicNode *parse_logic(char *str);
void free_logic_list(LogicNode *head);

job *parse_job(char *str);

#endif