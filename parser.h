#ifndef PARSER_H
#define PARSER_H

#include "dynamicstring.h"

typedef enum{
    SEP_SYNC,  // ;
    SEP_ASYNC, // &
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

typedef enum{
    REDIR_IN,     // [n] < f   (O_RDONLY)
    REDIR_OUT,    // [n] > f   (O_CREAT | O_TRUNC | O_WRONLY)
    REDIR_APPEND, // [n] >> f  (O_CREAT | O_APPEND | O_WRONLY)
    REDIR_RDWR,   // [n] <> f  (O_CREAT | O_RDWR)
    REDIR_DUP_IN, // [n] <& m  (dup m to n)
    REDIR_DUP_OUT, // [n] &> m  (dup n to m)
} RedirType;

typedef struct RedirNode{
    RedirType type;
    int target_fd; // [n]
    char *filename; // f
    struct RedirNode *next;
} RedirNode;

RedirNode *parse_redir(char **args);
void free_redir_list(RedirNode *head);

typedef struct CmdNode{
    char **args;
    RedirNode *redirs;
} CmdNode;

CmdNode *parse_cmd(char **args);
void free_cmd(CmdNode *node);

#endif