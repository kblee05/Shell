#include "my_shell.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

//  =====================Helper functions====================

/*
 *  check whether cmd is builtin or not
 */

static int is_builtin(char *arg){
    if(!strcmp(arg, "cd")) return 1;
    if(!strcmp(arg, "quit")) return 1;
    return 0;
}

/*
 *  Check if argument is a cmdlist operator
 */

static int is_cmdlistop(const char* arg){
    if(!arg) return 0;
    if(!strcmp(arg, ";")) return 1;
    if(!strcmp(arg, "&&")) return 1;
    if(!strcmp(arg, "||")) return 1;
    return 0;
}

/*
 *  Check whether command results and cmdlist operator relation is valid
 *  op: ;, &&, ||
 */

static int prev_status(const char* op, const int status){
    if(!is_cmdlistop(op))
        return 0;
    
    if(!strcmp(op, ";")){
        return 1;
    }
    if(!strcmp(op, "&&")){
        return !status;
    }
    if(!strcmp(op, "||")){
        return status;
    }
    return 1;
}

/*
 *  Debug char** types
 */

static void debug_args(char **args){
    size_t i = 0;
    printf("Args: ");
    while(args[i]){
        printf("%s | ", args[i++]);
    }
    printf("\n");
}

//  ==========================================================


/*
 *  Start of the shell
 */

static int exec_cmdlist(char **args);

void myshell_loop(){
    int status;
    do{
        printf("< ");
        char *line;
        while(!((line) = readline())){}
        char **args = parseline(line);
        debug_args(args);
        status = exec_cmdlist(args);
        
        free(line);
        size_t i = 0;
        while(args[i])
            free(args[i++]);
        free(args);
    }while(!status);
}



/*
 *  Execute the arguments
 *  op = ; OR && or ||
 *  cmdlist = cmd { op cmdlist }
 *  
 *  Function hiearchy goes like LOOP -> EXEC_CMDLIST -> 
 */

#define SHELL_TOK_BUFSIZE 64

static int exec_cmd(char **args);

static int exec_cmdlist(char **args){
    size_t pos = 0;
    int status;
    do{
        char **new_args = malloc(sizeof(char *) * SHELL_TOK_BUFSIZE);
        size_t idx = 0;
        int depth = 0;

        while(args[pos]){
            if(strcmp(args[pos], "(") == 0) depth++;
            else if(strcmp(args[pos], ")") == 0) depth--;

            if(depth == 0 && is_cmdlistop(args[pos])) break;

            new_args[idx++] = args[pos++];
        }
        new_args[idx] = NULL;

        status = exec_cmd(new_args);
        free(new_args);
    }while(args[pos] != NULL && prev_status(args[pos++], status));
    
    // end has to be either null or an op
    // return 0 if finished successfully
    return args[pos] && !is_cmdlistop(args[pos-1]);
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

static char **rm_parens(char **args);

static int exec_subshell(char **args){
    char **new_args = rm_parens(args);

    printf("subshell new args: ");
    debug_args(new_args);

    pid_t pid = fork();
    if(pid < 0){
        perror("Shell: subshell fork failed\n");
        exit(1);
    }
    else if(pid == 0){ // subshell
        int status = exec_cmdlist(new_args);
        exit(status);
    }
    else{ // parent
        int status;
        pid_t wpid = waitpid(pid, &status, WUNTRACED);
        size_t i = 0;
        while(new_args[i])
            free(new_args[i++]);
        free(new_args);
        if(wpid)
            return WEXITSTATUS(status);
        return 1;
    }
}

/*
 *  remove parenthesis tokens for subshell operations
 */

static char **rm_parens(char **args){
    if(strcmp(args[0], "(")){
        perror("Shell: subshell does not start with paranthesis\n");
        exit(1);
    }

    size_t bufsize = SHELL_TOK_BUFSIZE;
    char **new_args = malloc(sizeof(char *) * bufsize);
    size_t i = 1; // skip opening paranthesis

    while(args[i] && strcmp(args[i], ")")){
        if(i >= bufsize - 1){
            bufsize += SHELL_TOK_BUFSIZE;
            new_args = realloc(new_args, bufsize * sizeof(char *));
        }

        new_args[i-1] = strdup(args[i]);
        i++;
    }
    if(strcmp(args[i], ")")){
        perror("Shell: subshell did not end with paranthesis\n");
        exit(1);
    }

    new_args[i-1] = NULL;
    return new_args;
}



/*
 *  execute builtin commands
 */

static int exec_builtin(char **args){
    if(!strcmp(args[0], "cd")){
        int res = chdir(args[1]);
        char **cd_arg = malloc(sizeof(char *) * 2);
        cd_arg[0] = "pwd";
        cd_arg[1] = NULL;
        exec_external(cd_arg);
        free(cd_arg);
        return res;
    }
    else if(!strcmp(args[0], "quit")){
        exit(0);
    }
    return 1;
}

/*
 *  Execute commands that require fork - exec
 */

static int exec_external(char **args){
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

