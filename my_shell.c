#include "my_shell.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

#define SHELL_RL_BUFSIZE 1024

char *myshell_readline(){
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

#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_LENGTH 1024
#define SHELL_TOK_DELIM " \t\r\n\a"

static size_t tok_bufsize = 0;

static void add_token(char* token, size_t *t_idx, char **tokens, size_t *pos){
    token[*t_idx] = '\0';
    *t_idx = 0;
    tokens[(*pos)++] = strdup(token);
    tokens[*pos] = NULL;
}

static char **myshell_parseline(char* line){
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

static void myshell_launch(char **args){
    pid_t pid = fork();

    if(pid < 0){
        perror("Shell: fork failed\n");
        exit(1);
    }
    else if(pid == 0){ // Child process
        if(execvp(args[0], args) == -1){
            perror("Shell: fork child failed\n");
            exit(1);
        }
        exit(0);
    }
    else{ // Parent process
        pid_t wpid;
        int status;
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

static int myshell_execute(char **args){
    if(!strcmp(args[0], "cd")){
        chdir(args[1]);
        char **cd_arg = malloc(sizeof(char *) * 2);
        cd_arg[0] = "pwd\0";
        cd_arg[1] = NULL;
        myshell_launch(cd_arg);
        free(cd_arg);
    }
    else{
        myshell_launch(args);
    }
    return 1;
}

void myshell_loop(){
    int status;
    do{
        printf("< ");
        char *line;
        while(!((line) = myshell_readline())){}
        char **args = myshell_parseline(line);
        status = myshell_execute(args);
        
        free(line);
        free(args);
    }while(status);
}