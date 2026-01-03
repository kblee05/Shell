#include "my_shell.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

//  =====================Helper functions====================

/*
 *  Token helper function
 */

static void add_token(char* token, size_t *t_idx, char **tokens, size_t *pos){
    token[*t_idx] = '\0';
    *t_idx = 0;
    tokens[(*pos)++] = strdup(token);
    tokens[*pos] = NULL;
}

/*
 *  check whether cmd is builtin or not
 */

static int is_builtin(char *arg){
    if(!strcmp(arg, "cd")) return 1;
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

//  ==========================================================

/*  
 *  ======================
 *  Part for reading input
 *  ======================
 */ 

#define SHELL_RL_BUFSIZE 1024

/*
 *  Buffer for termianl commands
 */

static char *readline(){
    int bufsize = SHELL_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    
    if(!buffer){
        perror("Shell: rl buffer allocation failed\n");
        return NULL;
    }

    while(1){
        int c = getchar();

        if(c == EOF || c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }

        buffer[position++] = (char) c;

        if(position >= bufsize){
            bufsize += SHELL_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if(!buffer){
                perror("Shell: rl buffer reallocationf failed\n");
                return NULL;
            }
        }
    }
}

/*  
 *  ================
 *  Part for parsing
 *  ================
 */ 

#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_LENGTH 1024
#define SHELL_TOK_DELIM " \t\r\n\a"

static size_t tok_bufsize = 0;

/*
 *  Parser
 */

static char **parseline(char* line){
    tok_bufsize = SHELL_TOK_BUFSIZE;
    char **tokens = malloc(tok_bufsize * sizeof(char *));
    if(!tokens){
        perror("Shell: token allocation failed\n");
        exit(1);
    }

    char *token = malloc(SHELL_TOK_LENGTH);
    if(!token){
        perror("Shell: temp token buf allocation failed\n");
        exit(1);
    }

    size_t t_idx = 0;
    size_t pos = 0;
    int is_empty = 0;
    
    for(int i = 0; line[i] != '\0'; i++){
        if(pos >= tok_bufsize - 1){
            tok_bufsize += SHELL_TOK_BUFSIZE;
            tokens = realloc(tokens, tok_bufsize);
        }

        char c = line[i];

        if(isspace(c)){
            if(t_idx || is_empty) add_token(token, &t_idx, tokens, &pos);
            is_empty = 0;
            continue;
        }

        if(c == '\\'){
            i++;
            char next = line[i];
            if(next == '\n'){
                continue;
            }
            else if(next != '\0'){
                token[t_idx++] = next;
            }
        }
        else if(c == '\''){
            is_empty = 1;
            i++;
            while(line[i] != '\0' && line[i] != '\''){
                token[t_idx++] = line[i++];
            }

            if(line[i] == '\0'){
                perror("Shell: single quote not finished\n");
                exit(1);
            }
        }
        else if(c == '\"'){
            is_empty = 1;
            i++;
            while(line[i] != '\0' && line[i] != '\"'){
                if(line[i] == '\\'){
                    char next = line[i + 1];
                    if(next == '$' || next == '`' || next == '\"' || next == '\\' || next == '\n'){
                        i++;
                    }
                }
                token[t_idx++] = line[i++]; 
            }

            if(line[i] == '\0'){
                perror("Shell: double quote not finished\n");
                exit(1);
            }
        }
        else if(c == '$'){
            i++;
            char var_name[64];
            int v_idx = 0;

            while(isalnum(line[i]) || line[i] == '_'){
                var_name[v_idx++] = line[i++];
            }
            var_name[v_idx] = '\0';
            i--;

            char *val = getenv(var_name);
            if(val){
                while(*val){
                    token[t_idx++] = *val++;
                }
            }
        }
        else{
            token[t_idx++] = c;
        }
    }

    if(t_idx || is_empty){
        add_token(token, &t_idx, tokens, &pos);
    }
    
    free(token);
    return tokens;
}

/*  
 *  ===============================
 *  Part for execution of arguments
 *  ===============================
 */ 

static int exec_builtin(char **args);
static int exec_cmd(char **args);

/*
 *  execute builtin commands
 */

static int exec_builtin(char **args){
    if(!strcmp(args[0], "cd")){
        int res = chdir(args[1]);
        char **cd_arg = malloc(sizeof(char *) * 2);
        cd_arg[0] = "pwd";
        cd_arg[1] = NULL;
        exec_cmd(cd_arg);
        free(cd_arg);
        return res;
    }
    return 1;
}

/*
 *  Execute commands that require fork - exec
 */

static int exec_cmd(char **args){
    if(!args[0])
        return 0;

    if(is_builtin(args[0]))
        return exec_builtin(args);
    
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

/*
 *  Execute the arguments
 *  op = ; OR && or ||
 *  cmdlist = cmd { op cmdlist }
 *  
 *  Function hiearchy goes like LOOP -> EXEC_CMDLIST -> 
 */

static int exec_cmdlist(char **args){
    size_t pos = 0;
    int status;
    do{
        char **new_args = malloc(sizeof(char *) * SHELL_TOK_BUFSIZE);
        size_t idx = 0;
        while(args[pos] && !is_cmdlistop(args[pos])){
            new_args[idx++] = args[pos++];
        }
        new_args[idx] = NULL;
        status = exec_cmd(new_args);
        free(new_args);
    }while(args[pos] != NULL && prev_status(args[pos++], status));
    
    return !args[pos] || is_cmdlistop(args[pos-1]);
}

/*
 *  Start of the shell
 */

void myshell_loop(){
    int status;
    do{
        printf("< ");
        char *line;
        while(!((line) = readline())){}
        char **args = parseline(line);
        status = exec_cmdlist(args);
        
        free(line);
        free(args);
    }while(status);
}