#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "jobcontrol.h"
#include "my_shell.h"

/*
 * ==============================================================
 *
 *                Parse separators ;, &
 * 
 * ==============================================================
 */


static void
append_sepnode(SepNode **head, char *cmd, SepType type)
{
    SepNode *new_node = (SepNode *)malloc(sizeof(SepNode));
    if(new_node == NULL) return;

    new_node->cmd = cmd;
    new_node->type = type;
    new_node->next = NULL;

    if(*head == NULL)
    {
        *head = new_node;
        return;
    }

    SepNode *curr = *head;
    while(curr->next)
        curr = curr->next;
    curr->next = new_node;
}

SepNode *
parse_sep(char *str)
{
    SepNode *head = NULL;
    dystring ds;
    init_dystring(&ds);
    int depth = 0;

    for(int i=0; str[i]; i++)
    {
        if(str[i] == '(') depth++;
        else if(str[i] == ')') depth--;

        if(depth > 0 || (str[i] == ')' && depth == 0))
        {
            append_dystring(&ds, str[i]);
            continue;
        }

        if(str[i] == ';' || (str[i] == '&' && (str[i+1] != '&' && str[i-1] != '&' && str[i-1] != '<' && str[i+1] != '>')))
        {
            SepType type = str[i] == ';' ? SEP_SYNC : SEP_ASYNC;
            append_sepnode(&head, ds.string, type);
            init_dystring(&ds);
            continue;
        }

        append_dystring(&ds, str[i]);
    }

    if(ds.curr_size > 0)
        append_sepnode(&head, ds.string, SEP_SYNC);
    else
        free(ds.string);

    return head;
}

void
free_sep_list(SepNode *head)
{
    SepNode *curr = head;
    while(curr != NULL){
        SepNode *next = curr->next;
        free(curr->cmd);
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
append_logicnode(LogicNode **head, char *cmd, LogicType type)
{
    LogicNode *new_node = (LogicNode *)malloc(sizeof(LogicNode));
    if(new_node == NULL) return;

    new_node->cmd = cmd;
    new_node->type = type;
    new_node->next = NULL;

    if(*head == NULL)
    {
        *head = new_node;
        return;
    }

    LogicNode *curr = *head;
    while(curr->next)
        curr = curr->next;

    curr->next = new_node;
}

LogicNode *
parse_logic(char *str)
{
    LogicNode *head = NULL;
    dystring ds;
    init_dystring(&ds);
    int depth = 0;

    for(int i=0; str[i]; i++)
    {
        if(str[i] == '(') depth++;
        else if(str[i] == ')') depth--;

        if(depth > 0 || (str[i] == ')' && depth == 0)){
            append_dystring(&ds, str[i]);
            continue;
        }

        if((str[i] == '&' && str[i + 1] == '&') || 
           (str[i] == '|' && str[i + 1] == '|'))
        {
            LogicType type = str[i] == '&' ? LOGIC_AND : LOGIC_OR;
            append_logicnode(&head, ds.string, type);
            init_dystring(&ds);
            i++;
            continue;
        }

        append_dystring(&ds, str[i]);
    }

    if(ds.curr_size > 0)
        append_logicnode(&head, ds.string, LOGIC_NONE);
    else
        free(ds.string);

    return head;
}

void
free_logic_list(LogicNode *head)
{
    LogicNode *curr = head;
    while(curr != NULL){
        LogicNode *next = curr->next;
        free(curr->cmd);
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

static int
is_redirec(char *arg)
{
    if(arg == NULL || arg[0] == '\0')
        return 0;
    
    int start = 0;

    while(arg[start] >= '0' && arg[start] <= '9')
        start ++;
    
    if(!strncmp(&arg[start], ">>", 2) ||
       !strncmp(&arg[start], "<>", 2) ||
       !strncmp(&arg[start], "<&", 2) ||    
       !strncmp(&arg[start], "&>", 2) ||
       !strncmp(&arg[start], "<&-", 3))
        return 1;
    
    if(arg[start] == '<' || arg[start] == '>')
        return 1;
    
    return 0;
}

static void
token_process(process *p, char *cmd)
{
    dystring ds;
    init_dystring(&ds);
    char **argv = malloc(sizeof(char*) * 16);
    int arg_idx = 0;
    int depth = 0;

    for(int i=0; cmd[i]; i++)
    {
        if(cmd[i] == '(') depth++;
        else if(cmd[i] == ')') depth--;

        if(depth > 0 || (cmd[i] == ')' && depth == 0)){
            append_dystring(&ds, cmd[i]);
            continue;
        }

        if(is_redirec(ds.string) && (cmd[i] != '>' && cmd[i] != '<' && cmd[i] != '&'))
        {
            argv[arg_idx++] = ds.string;
            init_dystring(&ds);
            i--; // filename might not be separated by whitespace
            continue;
        }

        if(isspace(cmd[i]))
        {
            if(ds.curr_size > 0)
            {
                argv[arg_idx++] = ds.string;
                init_dystring(&ds);
            }
            continue;
        }

        append_dystring(&ds, cmd[i]);
    }

    if(ds.curr_size > 0)
        argv[arg_idx++] = ds.string;
    argv[arg_idx] = NULL;

    int p_idx = 0;
    redirection **last = &p->redirs;

    for(int i=0; argv[i] != NULL; i++)
    {
        if(is_redirec(argv[i]))
        {
            redirection *r = malloc(sizeof(redirection));
            r->fd_source = -1;
            r->next = NULL;
            int j;
            int sum = 0;

            for(j = 0; argv[i][j] >= '0' && argv[i][j] <= '9'; j++)
            {
                sum *= 10;
                sum += argv[i][j] - '0';
            }
            if(sum > 0)
                r->fd_source = sum;

            char *op = &argv[i][j];

            if(strlen(op) == 1)
            {
                r->type = REDIR_FILE;
                if(*op == '<')
                {
                    if(r->fd_source == -1)
                        r->fd_source = 0;
                    r->flags = O_RDONLY; 
                }
                else // op == '<'
                {
                    if(r->fd_source == -1)
                        r->fd_source = 1;
                    r->flags = O_CREAT | O_TRUNC | O_WRONLY;
                }
            }
            else
            {
                if(!strncmp(op, ">>", 2))
                {
                    r->type = REDIR_FILE;
                    if(r->fd_source == -1)
                        r->fd_source = 1;
                    r->flags = O_CREAT | O_APPEND | O_WRONLY;
                }
                else if(!strncmp(op, "<>", 2))
                {
                    r->type = REDIR_FILE;
                    if(r->fd_source == -1)
                        r->fd_source = 0;
                    r->flags = O_CREAT | O_RDWR;
                }
                else if(!strncmp(op, "<&", 2))
                {
                    r->type = REDIR_DUP;
                    if(r->fd_source == -1)
                        r->fd_source = 0;
                }
                else if(!strncmp(op, "&>", 2))
                {
                    r->type = REDIR_DUP;
                    if(r->fd_source == -1)
                        r->fd_source = 1;
                }
            }

            r->filename = argv[++i];
            if(!strncmp(r->filename, "-", 1))
                r->type = REDIR_CLOSE;
            if(*last)
                (*last)->next = r;
            else
                *last = r;
            continue;
        }

        p->argv[p_idx++] = argv[i];
    }
}

void
add_process(job *j, char *cmd)
{
    process *p = malloc(sizeof(process));
    p->argv = malloc(sizeof(char *) * 16);
    p->next = NULL;
    p->pid = -1;
    p->completed = 0;
    p->stopped = 0;
    p->status = -1;
    p->redirs = NULL;
    token_process(p, cmd);

    if(j->first_process == NULL)
        j->first_process = p;
    else
    {
        process *last = j->first_process;
        for(; last->next; last = last->next){}
        last->next = p;
    }
}


job *
parse_job(char *str)
{
    job *j = calloc(1, sizeof(job));
    j->first_process = NULL;
    j->command = strdup(str);
    j->notified = 0;
    j->stdin = STDIN_FILENO;
    j->stdout = STDOUT_FILENO;
    j->stderr = STDERR_FILENO;
    j->status = -1;
    j->tmodes = shell_tmodes;

    if(shell_is_interactive)
        j->tmodes = shell_tmodes;

    dystring ds;
    init_dystring(&ds);
    int depth = 0;

    for(int i=0; str[i]; i++){
        if(str[i] == '(') depth++;
        else if(str[i] == ')') depth--;

        if(depth > 0 || (str[i] == ')' && depth == 0)){
            append_dystring(&ds, str[i]);
            continue;
        }
        
        if(str[i] == '|'){
            add_process(j, ds.string);
            free(ds.string);
            init_dystring(&ds);
            continue;
        }
        
        append_dystring(&ds, str[i]);
    }

    if(ds.curr_size > 0){
        add_process(j, ds.string);
        free(ds.string);
    }

    return j;
}



/*
 * ==============================================================
 *
 *                       Parse redirections >
 * 
 * ==============================================================
 */

 /*
static void
append_redirnode(RedirNode **head, RedirType type, int target_fd, char *filename)
{
    RedirNode *new_node = (RedirNode *)malloc(sizeof(RedirNode));
    if(new_node == NULL) return;

    new_node->type = type;
    new_node->target_fd = target_fd;
    new_node->filename = filename;
    new_node->next = NULL;

    if(*head == NULL){
        *head = new_node;
        return;
    }

    RedirNode *curr = *head;
    while(curr->next != NULL){
        curr = curr->next;
    }

    curr->next = new_node;
}

RedirNode
*parse_redir(char **args)
{
    RedirNode *head = NULL;
    int depth = 0;
    
    for(int i=0; args[i] != NULL; i++){
        if (strcmp(args[i], "(") == 0) { depth++; continue; }
        if (strcmp(args[i], ")") == 0) { depth--; continue; }
        if (depth > 0) continue;

        if(!is_redir(args[i])){ continue; }

        RedirType type;
        int target_fd = -1;
        char *filename = NULL;
        int start = 0;

        if(isdigit(args[i][0])){
            target_fd = args[i][0] - '0';
            start = 1;
        }

        if(strcmp(&args[i][start], "<") == 0){
            type = REDIR_IN;
            if(target_fd == -1){
                target_fd = 0;
            }
        }
        else if(strcmp(&args[i][start], ">") == 0){
            type = REDIR_OUT;
            if(target_fd == -1){
                target_fd = 1;
            }
        }
        else if(strcmp(&args[i][start], ">>") == 0){
            type = REDIR_APPEND;
            if(target_fd == -1){
                target_fd = 1;
            }
        }
        else if(strcmp(&args[i][start], "<>") == 0){
            type = REDIR_RDWR;
            if(target_fd == -1){
                target_fd = 0;
            }
        }
        else if(strcmp(&args[i][start], "<&") == 0){
            type = REDIR_DUP_IN;
            if(target_fd == -1){
                target_fd = 0;
            }
        }
        else if(strcmp(&args[i][start], "&>") == 0){
            type = REDIR_DUP_OUT;
            if(target_fd == -1){
                target_fd = 1;
             }
        }

        if(args[i + 1] != NULL){
            filename = strdup(args[++i]);
        }

        append_redirnode(&head, type, target_fd, filename);
    }

    return head;
}

void
free_redir_list(RedirNode *head)
{
    RedirNode *curr = head;
    while(curr != NULL){
        RedirNode *next = curr->next;
        
        if(curr->filename != NULL){
            free(curr->filename);
        }

        free(curr);
        curr = next;
    }
}
*/
/*
 * ==============================================================
 *
 *                      Parse simple commands
 * 
 * ==============================================================
 */
/*
CmdNode
*parse_cmd(char **args)
{
    CmdNode *cmd = (CmdNode *)malloc(sizeof(CmdNode));
    cmd->redirs = NULL;
    dystring ds;
    init_dystring(&ds);
    int depth = 0;

    for(int i=0; args[i] != NULL; i++){
        if(strcmp(args[i], "(") == 0) depth++;
        else if(strcmp(args[i], ")") == 0) depth--;

        if(depth > 0 || (strcmp(args[i], ")") == 0 && depth == 0)){
            append_dystring(&ds, args[i]);
            continue;
        }

        if(is_redir(args[i])){
            i++; // skip redir operator and filename
            if(args[i] == NULL) break;
            continue;
        }

        append_dystring(&ds, args[i]);
    }
    if(ds.curr_size > 0){
        cmd->args = ds.strings;
    }
    else{
        free(ds.strings);
        free(cmd); // no cmd
        return NULL;
    }

    return cmd;
}

void
free_cmd(CmdNode *node)
{
    if(node->args != NULL){
        for(int i=0; node->args[i] != NULL; i++){
            free(node->args[i]);
        }
        free(node->args);
    }

    free_redir_list(node->redirs);
    free(node);
}
    */