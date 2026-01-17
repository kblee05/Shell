#include "tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

/*  
 *  ======================
 *  Part for reading input
 *  ======================
 */ 

#define SHELL_RL_BUFSIZE 1024

/*
 *  Buffer for termianl commands
 */

char *readline(){
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