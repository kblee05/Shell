#include "parser.h"
#include <stdlib.h>
#include <string.h>

/*
 * ==============================================================
 *
 *                Parse separators ;, &
 * 
 * ==============================================================
 */


static void
append_sepnode(SepNode **head, char **args, SepType type)
{
    SepNode *new_node = (SepNode *)malloc(sizeof(SepNode));
    if(new_node == NULL) return;

    new_node->args = args;
    new_node->type = type;
    new_node->next = NULL;

    if(*head == NULL){
        *head = new_node;
        return;
    }

    SepNode *curr = *head;
    while(curr->next != NULL){
        curr = curr->next;
    }

    curr->next = new_node;
}

SepNode
*parse_sep(char **args)
{
    SepNode *head = NULL;
    dystring ds;
    init_dystring(&ds);
    char **sep_args;
    int depth = 0;

    for(int i=0; args[i] != NULL; i++){
        if(strcmp(args[i], "(") == 0) depth++;
        else if(strcmp(args[i], ")") == 0) depth--;

        if(depth > 0){
            append_dystring(&ds, args[i]);
            continue;
        }

        if(strcmp(args[i], ";") == 0 || strcmp(args[i], "&") == 0){
            sep_args = ds.strings; // move ownership
            ds.strings = NULL; // reset dangling pointer
            init_dystring(&ds);

            SepType type = strcmp(args[i], ";") == 0 ? SEP_SYNC : SEP_ASYNC;
            append_sepnode(&head, sep_args, type);
            continue;
        }
        append_dystring(&ds, args[i]);
    }
    if(ds.curr_size > 0){
        append_sepnode(&head, ds.strings, SEP_NONE);
    }
    else{
        free(ds.strings);
        ds.strings = NULL;
    }

    return head;
}

void
free_sep_list(SepNode *head)
{
    SepNode *curr = head;
    while(curr != NULL){
        SepNode *next = curr->next;
        
        if(curr->args != NULL){
            for(int i=0; curr->args[i] != NULL; i++){
                free(curr->args[i]);
            }
            free(curr->args);
        }

        free(curr);
        curr = next;
    }
}

/*
 * ==============================================================
 *
 *                Parse Logic operators &&, ||
 * 
 * ==============================================================
 */

static void
append_logicnode(LogicNode **head, char **args, LogicType type)
{
    LogicNode *new_node = (LogicNode *)malloc(sizeof(LogicNode));
    if(new_node == NULL) return;

    new_node->args = args;
    new_node->type = type;
    new_node->next = NULL;

    if(*head == NULL){
        *head = new_node;
        return;
    }

    LogicNode *curr = *head;
    while(curr->next != NULL){
        curr = curr->next;
    }

    curr->next = new_node;
}

LogicNode
*parse_logic(char **args)
{
    LogicNode *head = NULL;
    dystring ds;
    init_dystring(&ds);
    char **sep_args;
    int depth = 0;

    for(int i=0; args[i] != NULL; i++){
        if(strcmp(args[i], "(") == 0) depth++;
        else if(strcmp(args[i], ")") == 0) depth--;

        if(depth > 0){
            append_dystring(&ds, args[i]);
            continue;
        }

        if(strcmp(args[i], "&&") == 0 || strcmp(args[i], "||") == 0){
            sep_args = ds.strings; // move ownership
            ds.strings = NULL; // reset dangling pointer
            init_dystring(&ds);

            LogicType type = strcmp(args[i], "&&") == 0 ? LOGIC_AND : LOGIC_OR;
            append_logicnode(&head, sep_args, type);
            continue;
        }
        append_dystring(&ds, args[i]);
    }
    if(ds.curr_size > 0){
        append_logicnode(&head, ds.strings, LOGIC_NONE);
    }
    else{
        free(ds.strings);
        ds.strings = NULL;
    }

    return head;
}

void
free_logic_list(LogicNode *head){
    LogicNode *curr = head;
    while(curr != NULL){
        LogicNode *next = curr->next;
        
        if(curr->args != NULL){
            for(int i=0; curr->args[i] != NULL; i++){
                free(curr->args[i]);
            }
            free(curr->args);
        }

        free(curr);
        curr = next;
    }
}

/*
 * ==============================================================
 *
 *                        Parse piplines |
 * 
 * ==============================================================
 */

static void
append_pipenode(PipeNode **head, char **args)
{
    PipeNode *new_node = (PipeNode *)malloc(sizeof(PipeNode));
    if(new_node == NULL) return;

    new_node->args = args;
    new_node->next = NULL;

    if(*head == NULL){
        *head = new_node;
        return;
    }

    PipeNode *curr = *head;
    while(curr->next != NULL){
        curr = curr->next;
    }

    curr->next = new_node;
}

PipeNode
*parse_pipe(char **args)
{
    PipeNode *head = NULL;
    dystring ds;
    init_dystring(&ds);
    char **sep_args;
    int depth = 0;

    for(int i=0; args[i] != NULL; i++){
        if(strcmp(args[i], "(") == 0) depth++;
        else if(strcmp(args[i], ")") == 0) depth--;

        if(depth > 0){
            append_dystring(&ds, args[i]);
            continue;
        }
        
        if(strcmp(args[i], "|") == 0){
            sep_args = ds.strings; // move ownership
            ds.strings = NULL; // reset dangling pointer
            init_dystring(&ds);

            append_pipenode(&head, sep_args);
            continue;
        }
        append_dystring(&ds, args[i]);
    }
    if(ds.curr_size > 0){
        append_pipenode(&head, ds.strings);
    }
    else{
        free(ds.strings);
        ds.strings = NULL;
    }

    return head;
}

void 
free_pipe_list(PipeNode *head)
{
    PipeNode *curr = head;
    while(curr != NULL){
        PipeNode *next = curr->next;
        
        if(curr->args != NULL){
            for(int i=0; curr->args[i] != NULL; i++){
                free(curr->args[i]);
            }
            free(curr->args);
        }

        free(curr);
        curr = next;
    }
}