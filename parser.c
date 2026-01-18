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
#include <string.h>
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

static char **
tokenize_process(char *cmd)
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
            argv[arg_idx++] = ds.string; // ownership moved
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
    else
        free(ds.string);
    
    argv[arg_idx] = NULL;

    return argv;
}

typedef struct RedirRule
{
    char *op;
    int type;
    int flags;
    int default_fd;
} RedirRule;

#define NUM_OF_REDIR 6

static RedirRule redir_rules[] = {
    {">",  REDIR_FILE, O_WRONLY | O_CREAT | O_TRUNC,  1},
    {"<",  REDIR_FILE, O_RDONLY,                      0},
    {">>", REDIR_FILE, O_WRONLY | O_CREAT | O_APPEND, 1},
    {"<>", REDIR_FILE, O_RDWR | O_CREAT,              0},
    {"<&", REDIR_DUP,  0,                             0},
    {"&>", REDIR_DUP,  0,                             0},
    {NULL, 0, 0, 0}
};

static int
is_env(char *arg)
{
    for(int i = 0; arg[i]; i++)
        if(arg[i] == '=')
            return 1;
    return 0;
}

/*
static dyarray *
new_envp()
{
    dyarray *envp = new_dyarray();
    
    for(int i = 0; my_environ.str[i]; i++)
        append_dyarray(envp, my_environ.str[i]);
    
    return envp;
}
*/

static void
parse_process(process *p, char *cmd)
{
    char **argv = tokenize_process(cmd);
    int e_idx;
    int p_idx = 0;
    redirection **last = &p->redirs;
    dyarray *envp = new_dyarray();

    for(int i=0; argv[i] != NULL; i++)
    {
        if(is_env(argv[i]))
        {
            append_dyarray(envp, argv[i]);
            free(argv[i]);
            continue;
        }
        
        if(!is_redirec(argv[i]))
        {
            p->argv[p_idx++] = argv[i]; // ownership moved
            continue;
        }
        
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
        if(j) // not default fd
            r->fd_source = sum;

        char *op = &argv[i][j];

        for(j = 0; j < NUM_OF_REDIR; j++)
        {
            if(!strcmp(op, redir_rules[j].op))
            {
                *last = r;
                last = &r->next;
                r->type = redir_rules[j].type;
                r->filename = argv[++i]; // ownership moved
                if(r->fd_source == -1)
                    r->fd_source = redir_rules[j].default_fd;
                r->flags = redir_rules[j].flags;

                if(!strncmp(r->filename, "-", 1))
                    r->type = REDIR_CLOSE;
                
                free(argv[i-1]); // free operator
                break;
            }
        }
    }

    p->argv[p_idx] = NULL;
    p->envp = envp->str; // envp.str ownership is moved to p->envp
    free(envp); 
    free(argv);
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
    parse_process(p, cmd);

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