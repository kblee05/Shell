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
    struct and function for passing single quotes, double quotes, and parenthesis
    quotes have a higher hierarchy comared to parenthesis
*/

typedef struct
{
    int in_s;
    int in_d;
    int depth;
} Scanner;

static int
update_and_check_protected(Scanner *s, char c)
{
    if(c == '\'' && !s->in_d)
    {
        s->in_s = !s->in_s;
        return 1;
    }

    if(c == '\"' && !s->in_s)
    {
        s->in_d = !s->in_d;
        return 1;
    }

    if(s->in_s || s->in_d)
        return 1;
    
    if(c == '(') s->depth++;
    if(c == ')') s->depth--;

    if(s->depth > 0 || (c == ')' && s->depth == 0))
        return 1;
    
    return 0;
}

/*
    Code for specifically parsing & (foreground) and ; (background) symbols
*/

static void
append_sepnode(SepNode **head, char *cmd, char sync)
{
    SepNode *new_node = malloc(sizeof(SepNode));
    if(new_node == NULL) return;

    new_node->cmd = cmd;
    new_node->sync = sync;
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

#define SYNC 1
#define ASYNC 0

SepNode *
parse_sep(char *str)
{
    SepNode *head = NULL;
    dystring ds;
    init_dystring(&ds);
    Scanner scanner = {0, 0, 0};

    for(int i=0; str[i]; i++)
    {
        if(update_and_check_protected(&scanner, str[i]))
        {
            append_dystring(&ds, str[i]);
            continue;
        }

        if(str[i] == ';' || (str[i] == '&' && (str[i+1] != '&' && str[i-1] != '&' && str[i-1] != '<' && str[i+1] != '>')))
        {
            append_sepnode(&head, ds.string, str[i] == ';' ? SYNC : ASYNC); // move ownership
            init_dystring(&ds);
            continue;
        }

        append_dystring(&ds, str[i]);
    }

    if(ds.curr_size > 0)
        append_sepnode(&head, ds.string, SYNC); // move ownership
    else
        free(ds.string); // or just free

    return head;
}

void
free_sep_list(SepNode *head)
{
    SepNode *curr = head;

    while(curr != NULL)
    {
        SepNode *next = curr->next;
        free(curr->cmd);
        free(curr);
        curr = next;
    }
}

/*
    Code for specifically parsing && (AND) and || (OR) symbols
*/

static void
append_logicnode(LogicNode **head, char *cmd, LogicType type)
{
    LogicNode *new_node = malloc(sizeof(LogicNode));
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
    Scanner scanner = {0, 0, 0};

    for(int i=0; str[i]; i++)
    {
        if(update_and_check_protected(&scanner, str[i]))
        {
            append_dystring(&ds, str[i]);
            continue;
        }

        if((str[i] == '&' && str[i + 1] == '&') || 
           (str[i] == '|' && str[i + 1] == '|'))
        {
            LogicType type = str[i] == '&' ? LOGIC_AND : LOGIC_OR;
            append_logicnode(&head, ds.string, type); // move ownership
            init_dystring(&ds);
            i++; // skip latter char
            continue;
        }

        append_dystring(&ds, str[i]);
    }

    if(ds.curr_size > 0)
        append_logicnode(&head, ds.string, LOGIC_NONE); // move ownership
    else
        free(ds.string); // or free

    return head;
}

void
free_logic_list(LogicNode *head)
{
    LogicNode *curr = head;

    while(curr != NULL)
    {
        LogicNode *next = curr->next;
        free(curr->cmd);
        free(curr);
        curr = next;
    }
}

static char *find_unquoted(char *str, char c);
static char *find_unquoted_sub(char *str, char* sub);

/*
    Check whether current token is a redirection operator or not
*/

static int
is_redirec(char *str)
{
    if(find_unquoted_sub(str, ">>")) return 1;
    if(find_unquoted_sub(str, "&>")) return 1;
    if(find_unquoted_sub(str, "<>")) return 1;
    if(find_unquoted_sub(str, "<&")) return 1;
    if(find_unquoted_sub(str, ">")) return 1;
    if(find_unquoted_sub(str, "<")) return 1;
    
    return 0;
}

/*
    process tokens for normal execute arguments, 
    redirection operators/fileanmes, and environment variabel declaration
*/

static char **
tokenize_process(char *cmd)
{
    dystring ds;
    init_dystring(&ds);
    char **argv = malloc(sizeof(char*) * 64);
    int arg_idx = 0;
    Scanner scanner = {0, 0, 0};

    for(int i=0; cmd[i]; i++)
    {
        if(update_and_check_protected(&scanner, cmd[i]))
        {
            append_dystring(&ds, cmd[i]);
            continue;
        }

        if(is_redirec(ds.string) && (cmd[i] != '>' && cmd[i] != '<' && cmd[i] != '&'))
        {
            argv[arg_idx++] = ds.string; // ownership moved
            init_dystring(&ds);
            continue;
        }

        if(isspace(cmd[i]))
        {
            if(ds.curr_size > 0)
            {
                argv[arg_idx++] = ds.string; // move ownership
                init_dystring(&ds);
            }
            continue;
        }

        append_dystring(&ds, cmd[i]);
    }

    if(ds.curr_size > 0)
        argv[arg_idx++] = ds.string; // move ownership
    else
        free(ds.string); // or free
    
    argv[arg_idx] = NULL;

    return argv;
}

/*
    Struct for redirection cases >, <, >>, <>, &> (&>- for closing), <& 
*/

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

/*
    function for parsing redirection based on operator and filename string arguments
    need to free operator char * because we are not going to need the operator token
    for execution. all of the info of the operator is saved as metadata and only the
    filename should be preserved
*/

static redirection *
parse_redirec(char *redir, char *filename)
{
    redirection *r = new_redirection();
    int i;
    int sum = 0;
    int is_fd_default = 1;

    // extract the custom fd number if it exists
    for(i = 0; redir[i] >= '0' && redir[i] <= '9'; i++)
    {
        sum *= 10;
        sum += redir[i] - '0';
        is_fd_default = 0;
    }

    char *op = &redir[i]; // borrow

    for(i = 0; i < NUM_OF_REDIR; i++)
        if(!strcmp(op, redir_rules[i].op))
        {
            r->type = redir_rules[i].type;
            r->fd_source = redir_rules[i].default_fd;
            r->flags = redir_rules[i].flags;
            r->filename = filename; // ownership moved

            if(!strncmp(r->filename, "-", 1))
                r->type = REDIR_CLOSE;
            
            free(redir); // free operator token. only filename token is needed
            break;
        }
    
    
    if(!is_fd_default) // overwrite fd
        r->fd_source = sum;
    
    return r;
}

/*
    Function for parsing environment variables.
    Two arguments are needed: a pointer that points to the start
    of the environment variabel KEY, and a pointer that points to the
    original token.
    
    For example if the current token is "aaa/$FOO/bbb",
    the first argument is "$FOO/bbb", the second is
    "aaa/$FOO/bbb". if FOO=BAR, the result will be a new string
    with the value of "aaa/BAR/bbb".
*/

static void
expand_env(char **arg)
{
    char *env_var;
    char *res = strdup(*arg);

    while((env_var = strchr(res, '$')))
    {
        // prefix
        int pre_len = env_var - res;
        char *prefix = strdup(res);
        prefix[pre_len] = '\0';
        
        // value
        env_var = &env_var[1]; // skip $
        char *value;
        int var_len = 0;

        if(env_var[0] == '?') // exit status of last pipeline
        {
            value = malloc(12);
            snprintf(value, 12, "%d", last_exit_status);
            var_len = 1;
        }
        else if(env_var[0] == '$') // pid of current process that has terminal control
        {
            value = malloc(12);
            snprintf(value, 12, "%d", (int)getpid());
            var_len = 1;
        }
        else
        {
            while(env_var[var_len] && (isalnum(env_var[var_len]) || env_var[var_len] == '_'))
                var_len++;
            
            char *key = strdup(env_var); // dup FOO from ABC$FOO/DEF
            key[var_len] = '\0'; // only FOO\0 left
            value = get_environ(key); // malloc value
            free(key);
        }

        // suffix
        char *suffix = strdup(&res[pre_len + var_len + 1]);

        // new string with replaced env
        dystring *ds = new_dystring(); // malloc ds
        
        merge_dystring(ds, prefix);
        merge_dystring(ds, value ? value : "");
        merge_dystring(ds, suffix);
        free(prefix);
        free(value);
        free(suffix);

        free(res);
        res = ds->string; // move ownership of ds.string
        free(ds);
    }

    free(*arg);
    *arg = res; // move ownership
}

/*
    Remove single and double quotes from a token
    Every character must stay intact if it is within single quotes
    Environment variables within double quotes must be replaced
    with its value.
*/

static void
strip_quote(char **s) // manipulate *s (string)
{
    dystring ds;
    init_dystring(&ds);
    char *str = *s; // borrow
    
    for(int i = 0; str[i]; i++)
    {
        if(str[i] == '\'')
        {
            i++;
            while(str[i] && str[i] != '\'')
                append_dystring(&ds, str[i++]);
            continue;
        }

        if(str[i] == '\"')
        {
            i++;
            dystring dquote;
            init_dystring(&dquote);

            while(str[i] && str[i] != '\"')
                append_dystring(&dquote, str[i++]);
           
            expand_env(&dquote.string); // substitue environment variables

            merge_dystring(&ds, dquote.string); // duplication
            free(dquote.string);
            continue;
        }

        append_dystring(&ds, str[i]);
    }

    free(*s);
    *s = ds.string;
}

static char *
find_unquoted(char *str, char c)
{
    Scanner scanner = {0, 0, 0};
    int i;

    for(i = 0; str[i] ; i++)
    {
        if(update_and_check_protected(&scanner, str[i]))
            continue;
        
        if(str[i] == c)
            return &str[i];
    }

    return NULL;
}

static char *
find_unquoted_sub(char *str, char *sub)
{
    Scanner scanner = {0, 0, 0};
    int i;

    for(i = 0; str[i]; i++)
    {
        if(update_and_check_protected(&scanner, str[i]))
            continue;
        
        if(!strncmp(&str[i], sub, strlen(sub)))
            return &str[i];
    }

    return NULL;
}

static void
parse_process(process *p, char *cmd)
{
    char **argv = tokenize_process(cmd);
    int p_idx = 0;
    redirection **last = &p->redirs;
    dyarray *envp = new_dyarray();

    for(int i=0; argv[i] != NULL; i++)
    {   
        // subshell: do not substitue stuff within subshells!!!
        // commands like ( echo " ; " ) will break otherwise
        if (argv[i][0] == '(') {
            p->argv[p_idx++] = argv[i];
            continue;
        }
        
        if(find_unquoted(argv[i], '=')) // environment variable declaration
        {
            strip_quote(&argv[i]);
            append_dyarray(envp, argv[i]);
            free(argv[i]);
            continue;
        }

        if(is_redirec(argv[i]))
        {
            strip_quote(&argv[i]);
            strip_quote(&argv[i + 1]);
            *last = parse_redirec(argv[i], argv[i + 1]); // move ownership of r
            last = &(*last)->next;
            i++; // skip filename 
            continue;
        }
        
        expand_env(&argv[i]);

        strip_quote(&argv[i]);
        p->argv[p_idx++] = argv[i]; // ownership moved
    }

    p->argv[p_idx] = NULL;
    p->envp = envp->str; // envp.str ownership is moved to p->envp
    free(envp); 
    free(argv);
}

void
add_process(job *j, char *cmd)
{
    process *p = new_process();
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
    job *j = new_job();
    j->command = strdup(str);
    dystring ds;
    init_dystring(&ds);
    Scanner scanner = {0, 0, 0};

    for(int i=0; str[i]; i++){
        if(update_and_check_protected(&scanner, str[i]))
        {
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