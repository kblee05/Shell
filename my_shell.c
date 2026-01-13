#define _POSIX_C_SOURCE 200809L
#include "my_shell.h"
#include "parser.h"
#include "dynamicstring.h"
#include "tokenizer.h"
#include "sighandler.h"
#include "jobcontrol.h"
#include "sighandler.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

// https://github.com/tokenrove/build-your-own-shell/blob/master/stage_1.md

//  =====================Helper functions====================

#define SHELL_CMD_CNT 16
#define SHELL_TOK_BUFSIZE 64

volatile sig_atomic_t fg_child_count; // used for sigchld handler
volatile pid_t last_pid;
volatile int last_status;

/*
 *  check whether cmd is builtin or not
 */

static int
is_builtin(char *arg)
{
    if(!strcmp(arg, "cd")) return 1;
    if(!strcmp(arg, "quit")) return 1;
    return 0;
}

/*
 *  Debug char** types
 */

static void
debug_args(char **args)
{
    size_t i = 0;
    printf("\nArgs: ");
    while(args[i]){
        printf("%s, ", args[i++]);
    }
    printf("\n\n");
}

//  ==========================================================


/*
 *  Start of the shell
 */

static int exec_sep(char **args);

static sigset_t mask_all, mask_one, prev_one, prev_all;
pid_t sh_pgid;

void
myshell_loop()
{
    sigfillset(&mask_all);
    sigemptyset(&mask_one);
    sigaddset(&mask_one, SIGCHLD);
    signal_wrapper(SIGCHLD, sigchld_handler);
    sh_pgid = getpgrp();
    signal_wrapper(SIGINT, SIG_IGN);
    signal_wrapper(SIGTSTP, SIG_IGN);
    signal_wrapper(SIGTTIN, SIG_IGN);
    signal_wrapper(SIGTTOU, SIG_IGN);

    // init jobs

    int status;
    do{
        printf("< ");
        char *line;
        while(!((line) = readline())){}
        char **args = parseline(line);
        debug_args(args);

        if(strcmp(args[0], "jobs") == 0 || strcmp(args[0], "fg") == 0 || strcmp(args[0], "bg") == 0){
            exec_jobctrl(args);
        }
        else{
            status = exec_sep(args);
        }

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

static int exec_logic(char ** args, SepType sep_type);

static int
exec_sep(char **args)
{
    SepNode *curr = parse_sep(args);
    SepNode *head = curr;
    int res = 0;

    while(curr != NULL){
        res = exec_logic(curr->args, curr->type);
        curr = curr->next;
    }

    free_sep_list(head);
    return res;
}

/*
 *  execute logic operators && ||
 */

static int exec_pipe(char **args, SepType sep_type);

static int
exec_logic(char **args, SepType sep_type)
{
    LogicNode *curr = parse_logic(args);
    LogicNode *head = curr;
    int last_status = 0;
    int skip = 0;

    while(curr){
        if(!skip){
            last_status = exec_pipe(curr->args, sep_type);
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
//static int exec_cmd(char **args, SepType sep_type);
static void _exec_cmd_forked(char **args);
static int run_builtin(char **args);

// volatile sig_atomic_t child_count; // used for sigchld handler
// volatile sig_atomic_t fg_pgid;

static int
exec_pipe(char **args, SepType sep_type)
{
    if(args[0] != NULL && strcmp(args[0], "!") == 0){
        return (exec_pipe(&args[1], sep_type) == 0) ? 1 : 0;
    }
    
    PipeNode *curr = parse_pipe(args);
    PipeNode *head = curr;

    if(curr->next == NULL && is_builtin(curr->args[0])){ // single (no pipe) builtin cmd -> DO NOT FORK
        int res = run_builtin(curr->args);
        free_pipe_list(head);
        return res;
    }

    int pipefd[2];
    int prev_pipefd = -1;
    pid_t pgid = -1;
    int j_idx = -1;
    fg_child_count = 0;

    sigprocmask(SIG_BLOCK, &mask_one, &prev_one); // block sig child BEFORE race(fork)

    while(curr != NULL){
        if(sep_type == SEP_SYNC){
            fg_child_count++;
        }

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
            sigprocmask(SIG_SETMASK, &prev_one, NULL);
            if(sep_type == SEP_SYNC){
                signal_wrapper(SIGINT, SIG_DFL);
                signal_wrapper(SIGTSTP, SIG_DFL);
                signal_wrapper(SIGTTIN, SIG_DFL);
                signal_wrapper(SIGTTOU, SIG_DFL);
            }

            if(pgid == -1){ // first child of the proccess -> leader of process group
                setpgid(0, 0);
                if(sep_type == SEP_SYNC){
                    tcsetpgrp(0, getpid());
                }
            }
            else{
                setpgid(0, pgid);
            }

            if(prev_pipefd != -1){
                dup2(prev_pipefd, STDIN_FILENO);
                close(prev_pipefd);
            }

            if(curr->next != NULL){
                close(pipefd[0]); // close pipe read
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            _exec_cmd_forked(curr->args);
        }
        else{ // parent
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            if(pgid == -1){
                pgid = pid; // save the first child proccess's pid / pgid
                j_idx = addjob(pid, (sep_type == SEP_SYNC) ? FG : BG, curr->args[0]);
                if(sep_type == SEP_SYNC){
                    tcsetpgrp(0, pgid);
                }
            }
            setpgid(pid, pgid);

            if(j_idx != -1 && jobs[j_idx].n_procs < MAX_PROCESS){
                jobs[j_idx].pids[jobs[j_idx].n_procs++] = pid;
            }

            sigprocmask(SIG_SETMASK, &prev_all, NULL);


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

    if(sep_type == SEP_ASYNC){ // background process
        sigprocmask(SIG_SETMASK, &prev_one, NULL);
        return 0; // regard background process as 'success'
    }

    while(fg_child_count > 0){ // foreground process
        sigsuspend(&prev_one);
    }
    tcsetpgrp(0, sh_pgid);
    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    
    free_pipe_list(head);
    return last_status;
}

/*
 *  Check whether command is for subshell, builtin, or external
 */

static int run_builtin(char **args);
static dystring *rm_parens(char **args);
static void redirec(RedirNode* node);

static void
_exec_cmd_forked(char **args)
{
    if(args[0] == NULL){
        exit(0);
    }
    
    CmdNode *cmd = parse_cmd(args);
    cmd->redirs = parse_redir(args);
    redirec(cmd->redirs);


    if(is_builtin(cmd->args[0])){
        int res = run_builtin(cmd->args);
        free_cmd(cmd);
        cmd = NULL;
        exit(res);
    }    
    
    if(!strcmp(cmd->args[0], "(")){ // subshell
        dystring *ds = rm_parens(args);
        int res = exec_sep(ds->strings);
        free_dystring(ds);
        ds = NULL;
        free_cmd(cmd);
        cmd = NULL;
        exit(res); // ds is freeded when fork() is reaped by parent
    }
    
    // external cmd
    if(execvp(cmd->args[0], cmd->args) == -1){
        perror("Shell: _cmd_forked execvp failed\n");
        exit(127); // command not found
    }
}

static void redirec(RedirNode* node){
    if(node == NULL){
        return;
    }
    
    // redirection exists
    RedirNode *curr = node;
    while(curr != NULL){
        int fd;
        int target_fd = curr->target_fd;

        switch (curr->type)
        {
        case REDIR_IN:
            fd = open(curr->filename, O_RDONLY);
            if(fd < 0){ perror("Shell: redirection failed\n"); return; }
            dup2(fd, target_fd);
            close(fd);
            break;
        case REDIR_OUT:
            fd = open(curr->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0){ perror("Shell: redirection failed\n"); return; }
            dup2(fd, target_fd);
            close(fd);
            break;
        case REDIR_APPEND:
            fd = open(curr->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if(fd < 0){ perror("Shell: redirection failed\n"); return; }
            dup2(fd, target_fd);
            close(fd);
            break;
        case REDIR_RDWR:
            fd = open(curr->filename, O_RDWR | O_CREAT, 0644);
            if(fd < 0){ perror("Shell: redirection failed\n"); return; }
            dup2(fd, target_fd);
            close(fd);
            break;
        case REDIR_DUP_IN:
        case REDIR_DUP_OUT:
            if(curr->filename[0] == '-'){
                close(target_fd);
            }
            else{
                int m = atoi(curr->filename);
                dup2(m, target_fd);
            }
            break;
        }

        curr = curr->next;
    }
}

/*
 *  execute builtin commands
 */

static int
run_builtin(char **args)
{
    if(!strcmp(args[0], "cd")){
        int res = chdir(args[1]);
        if(res == -1){
            printf("cd: %s: No such file or directory\n", args[1]);
            return 1;
        }
        return 0;
    }
    else if(!strcmp(args[0], "quit")){
        for(int i = 0; i<MAX_JOBS; i++){
            if(jobs[i].pgid != 0){
                kill(-jobs[i].pgid, SIGCONT);
                kill(-jobs[i].pgid, SIGHUP);
            }
        }
        exit(0);
    }
    return 1;
}

/*
 *  remove parenthesis tokens for subshell operations
 */

static dystring
*rm_parens(char **args)
{
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