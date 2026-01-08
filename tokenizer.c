#include "tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

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

/*  
 *  ================
 *  Part for parsing
 *  ================
 * 
 *  http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_03
 * 
 */ 

 

#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_LENGTH 1024
#define SHELL_TOK_DELIM " \t\r\n\a"

static size_t tok_bufsize = 0;

/*
 *  Parser
 */

char **parseline(char* line){
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
            tokens = realloc(tokens, tok_bufsize * sizeof(char *));
        }

        char c = line[i];

        if(isspace(c)){
            if(t_idx || is_empty) add_token(token, &t_idx, tokens, &pos);
            is_empty = 0;
            continue;
        }

        if(c == '(' || c == ')' || c == '!' || c == ';'){
            if(t_idx || is_empty) add_token(token, &t_idx, tokens, &pos);
            token[t_idx++] = c;
            add_token(token, &t_idx, tokens, &pos);
            is_empty = 0;
            continue;
        }
        else if(c == '&' && isspace(line[i + 1])){ // & : background processes
            if(t_idx || is_empty) add_token(token, &t_idx, tokens, &pos);
            token[t_idx++] = c;
            add_token(token, &t_idx, tokens, &pos);
            i++; // skip whitespace
            continue;
        }
        else if(c == '\\'){
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
        else if(c == '|'){
            if(t_idx > 0){
                add_token(token, &t_idx, tokens, &pos);
            }

            token[t_idx++] = line[i];
            if(line[i + 1] == '|'){
                token[t_idx++] = line[i++];
            }
            add_token(token, &t_idx, tokens, &pos);
            continue;
        }
        else if(c == '<' || c == '>' || c == '&'){
            if(t_idx > 0 && !(t_idx == 1 && isdigit(token[0]))){
                add_token(token, &t_idx, tokens, &pos);
            }

            token[t_idx++] = line[i];

            if(line[i+1] == '>' || line[i+1] == '&'){ // covers && too
                token[t_idx++] = line[++i];
            }
            add_token(token, &t_idx, tokens, &pos);
            continue;
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