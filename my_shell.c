#include "my_shell.h"
#include "MemoryAllocator.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define SHELL_RL_BUFSIZE 1024

char *myshell_readline(){
    int bufsize = SHELL_RL_BUFSIZE;
    int position = 0;
    char *buffer = my_malloc(sizeof(char) * bufsize);
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

        buffer[position++] = c;

        if(position >= bufsize){
            bufsize += SHELL_RL_BUFSIZE;
            buffer = my_realloc(buffer, bufsize);
            if(!buffer){
                perror("Shell: rl buffer reallocationf failed\n");
                return NULL;
            }
        }
    }
}

#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_LENGTH SHELL_RL_BUFSIZE
#define SHELL_TOK_DELIM " \t\r\n\a"

char **myshell_parseline(char* line){
    size_t bufsize = SHELL_TOK_BUFSIZE;
    char **tokens = my_malloc(bufsize * sizeof(char *));
    if(!tokens){
        perror("Shell: token allocation failed\n");
        return NULL;
    }

    char *temp_buf = my_malloc(SHELL_TOK_LENGTH);
    if(!temp_buf){
        perror("Shell: temp token buf allocation failed\n");
        return NULL;
    }

    size_t t_idx = 0;
    int in_quotes = 0;
    size_t pos = 0;
    
    for(int i = 0; line[i] != '\0'; i++){
        char c = line[i];

        if(c == '\\'){
            temp_buf[t_idx++] = line[++i];
            continue;
        }
        else if(c == '\''){
            i++;
            while(line[i] != '\'' && line[i] != '\0'){
                temp_buf[t_idx++] = line[i++];
            }
            continue;
        }

        if(c == '"'){
            in_quotes = !in_quotes;
            continue;
        }

        if(!in_quotes && isspace(c)){
            if(t_idx > 0){
                temp_buf[t_idx] = '\0';
                tokens[pos++] = my_strdup(temp_buf);
                t_idx = 0;
            }
        }
        else{
            temp_buf[t_idx++] = c;
        }
    }

    /*
    char *token = strtok(line, SHELL_TOK_DELIM);

    while(token){
        tokens[position++] = token;

        if(position >= bufsize){
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = my_realloc(tokens, bufsize * sizeof(char *));
            if(!tokens){
                perror("Shell: token reallocation failed\n");
                return NULL;
            }
        }

        token = strtok(NULL, SHELL_TOK_DELIM);
    }
    

    tokens[position] = NULL;
    return tokens;
    */
}

static int myshell_launch(char **args){
    pid_t pid = fork();
    if(pid < 0){
        perror("Shell: fork failed\n");
        return -1;
    }

    if(pid == 0){ // Child process
        if(execvp(args[0], args) == -1){
            perror("Shell: fork child failed\n");
            return -1;
        }
    }
    else{ // Parent process
        pid_t wpid;
        int status;
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int myshell_execute(char **args){

}

void myshell_loop(){
    int status;

    do{
        printf("< ");
        char* line = myshell_readline();
        char ** args = myshell_parseline(line);
        status = myshell_execute(args);
        
        my_free(line);
        my_free(args);
    }while(status);
}