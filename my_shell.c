#include "my_shell.h"
#include "parser.h"
#include "dynamicstring.h"
#include "tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

// https://github.com/tokenrove/build-your-own-shell/blob/master/stage_1.md

//  =====================Helper functions====================

#define SHELL_CMD_CNT 16
#define SHELL_TOK_BUFSIZE 64

/*
 *  check whether cmd is builtin or not
 */

static int is_builtin(char *arg){
    if(!strcmp(arg, "cd")) return 1;
    if(!strcmp(arg, "quit")) return 1;
    return 0;
}

/*
 *  Debug char** types
 */

static void debug_args(char **args){
    size_t i = 0;
    printf("\nArgs: ");
    while(args[i]){
        printf("%s | ", args[i++]);
    }
    printf("\n\n");
}

//  ==========================================================


/*
 *  Start of the shell
 */

static int exec_sep(char **args);

void myshell_loop(){
    int status;
    do{
        printf("< ");
        char *line;
        while(!((line) = readline())){}
        char **args = parseline(line);
        //debug_args(args);
        status = exec_sep(args);
        
        free(line);
        size_t i = 0;
        while(args[i])
            free(args[i++]);
        free(args);
    }while(1);
}


/*
 *  execute separators ; &
 */

static int exec_logic(char ** args);

static int
exec_sep(char **args)
{
    SepNode *curr = parse_sep(args);
    SepNode *head = curr;

    while(curr != NULL){
        if(exec_logic(curr->args) == 1){
            free_sep_list(head);
            return 1;
        }

        curr = curr->next;
    }

    free_sep_list(head);
    return 0;
}

/*
 *  execute logic operators && ||
 */

static int exec_pipe(char **args);

static int
exec_logic(char **args)
{
    LogicNode *curr = parse_logic(args);
    LogicNode *head = curr;
    int last_status = 0;
    int skip = 0;

    while(curr){
        if(!skip){
            last_status = exec_pipe(curr->args);
        }

        if(curr->type == LOGIC_AND){
            skip = (last_status != 0);
        }
        else if(curr->type == LOGIC_OR){
            skip = (last_status == 0);
        }
        else{
            skip = 0;
        }

        curr = curr->next;
    }

    free_logic_list(head);
    return last_status;
}

/*
 *  execute pipeline |
 */

//static int exec_redirec(char **args);
static int exec_cmd(char **args);

static int
exec_pipe(char **args)
{
    PipeNode *curr = parse_pipe(args);
    PipeNode *head = curr;

    if(curr->next == NULL){
        return exec_cmd(args);
    }

    int pipefd[2];
    int prev_pipefd = -1;
    pid_t last_pid;
    int child_count = 0;

    while(curr != NULL){
        child_count++;

        if(curr->next != NULL){
            if(pipe(pipefd) == -1){
                perror("Shell: pipe failed\n");
                return 1;
            }
        }

        pid_t pid = fork();
        last_pid = pid;
        if(pid < 0){
            perror("Shell: pipeline fork failed\n");
            return 1;
        }
        else if(pid == 0){
            if(prev_pipefd != -1){
                dup2(prev_pipefd, STDIN_FILENO);
                close(prev_pipefd);
            }

            if(curr->next != NULL){
                close(pipefd[0]); // close pipe read
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            int status = exec_cmd(curr->args);
            exit(status);
        }
        else{ // parent
            if(prev_pipefd != -1)
                close(prev_pipefd); // need to close prev_pipefd afterwards because it became a FD (pipefd[0])
            
            if(curr->next != NULL){
                close(pipefd[1]);
                prev_pipefd = pipefd[0]; // prev_pipefd becomes a FD here (pipefd[0])
            }

            // pipes are reset after this iteration
            // need to save pipefd[0] for chain
            // DO NOT CLOSE prev_pipefd for the next process input
        }

        curr = curr->next;
    }
    free_pipe_list(head);

    int status;
    int last_status;
    for(int i=0; i<child_count; i++){
        pid_t wpid = wait(&status);
        if(wpid == last_pid){
            if(WIFEXITED(status)){
                last_status = WEXITSTATUS(status);
            }
            else{
                last_status = 1;
            }
        }
    }
    
    return last_status;
}


/*
 *  Check whether command is for subshell, builtin, or external
 */

static int exec_builtin(char **args);
static int exec_external(char **args);
static int exec_subshell(char **args);

static int exec_cmd(char **args){
    if(!args[0])
        return 0;

    if(strcmp(args[0], "!") == 0){ // negation
        int status = exec_cmd(&args[1]);
        return (status == 0) ? 1 : 0;
    }

    if(!strcmp(args[0], "(")){ // subshell
        return exec_subshell(args);
    }
    else if(is_builtin(args[0])){
        return exec_builtin(args);
    }
    return exec_external(args);
}

/*
 *  execute    ( foo )   in a subshell
 */

static dystring *rm_parens(char **args);

static int exec_subshell(char **args){
    dystring *ds = rm_parens(args);

    pid_t pid = fork();
    if(pid < 0){
        perror("Shell: subshell fork failed\n");
        exit(1);
    }
    else if(pid == 0){ // subshell
        int status = exec_sep(ds->strings);
        exit(status);
    }
    else{ // parent
        int status;
        pid_t wpid = waitpid(pid, &status, WUNTRACED);
        
        free_dystring(ds);
        free(ds);
        ds = NULL;

        if(wpid)
            return WEXITSTATUS(status);
        return 1;
    }
}

/*
 *  remove parenthesis tokens for subshell operations
 */

static dystring *rm_parens(char **args){
    //debug_args(args);
    if(strcmp(args[0], "(") != 0){
        perror("Shell: subshell does not start with paranthesis\n");
        exit(1);
    }

    dystring *ds = (dystring *)malloc(sizeof(dystring));
    init_dystring(ds);
    int i = 1;
    int depth = 0;

    while(args[i]  && (depth >0 || strcmp(args[i], ")"))){
        if(strcmp(args[i], "(") == 0) depth++;
        else if(strcmp(args[i], ")") == 0) depth--;
        append_dystring(ds, args[i++]);
    }

    if(strcmp(args[i], ")") != 0){
        perror("Shell: subshell did not end with paranthesis\n");
        exit(1);
    }

    return ds;
}



/*
 *  execute builtin commands
 */

static int exec_builtin(char **args){
    if(!strcmp(args[0], "cd")){
        int res = chdir(args[1]);
        if(res == -1){
            return 1;
        }
        return 0;
    }
    else if(!strcmp(args[0], "quit")){
        exit(0);
    }
    return 1;
}

/*
 *  Execute commands that require fork - exec
 */

static int
exec_external(char **args)
{
    pid_t pid = fork();

    if(pid < 0){
        perror("Shell: fork failed\n");
        return 1;
    }
    else if(pid == 0){ // Child process
        if(execvp(args[0], args) == -1){
            perror("Shell: fork child failed\n");
            exit(1);
        }
        exit(0);
    }
    else{ // Parent process
        int status;
        waitpid(pid, &status, WUNTRACED);
        if(WIFEXITED(status)){
            return WEXITSTATUS(status);
        }
        return 1;
    }
}

