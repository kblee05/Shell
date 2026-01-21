#define _GNU_SOURCE
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


//  ==========================================================

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
sigset_t mask_chld, prev_chld;
int last_exit_status = -1;

static dyarray my_environ;

extern char **environ;

char *
get_environ(char* key)
{
    size_t len = strlen(key);

    for(int i = 0; my_environ.str[i]; i++)
        if(!strncmp(my_environ.str[i], key, len) && my_environ.str[i][len] == '=')
            return strdup(&my_environ.str[i][len + 1]);

    return NULL;
}

void
init_shell()
{
    shell_terminal = STDERR_FILENO;
    shell_is_interactive = isatty(shell_terminal);
    init_dyarray(&my_environ);
    
    for(int i = 0; environ[i]; i++)
        append_dyarray(&my_environ, environ[i]);

    if(shell_is_interactive)
    {
        while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);
        
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);
        
        shell_pgid = getpid();
        if(setpgid(shell_pgid, shell_pgid) < 0)
        {
            perror("Couldn't put shell in it's own process group");
            exit(1);
        }

        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);

        signal_wrapper(SIGCHLD, sigchld_handler);
        sigemptyset(&mask_chld);
        sigaddset(&mask_chld, SIGCHLD);
        sigprocmask(SIG_UNBLOCK, &mask_chld, NULL);
    }
}

/*
 *  Start of the shell
 */

static int exec_sep(char *str);

void
myshell_loop()
{
    init_shell();

    do
    {
        do_job_notification();
        printf("< ");
        char *line;
        while(!((line) = readline())){}

        if(strncmp(line, "jobs", 4) == 0)
        {
            job *j = first_job;
            for(; j; j = j->next)
                format_job_info(j, (j->first_process->stopped) ? "stopped" : "running");
        }
        else if(strncmp(line, "fg", 2) == 0)
        {
            pid_t pgid = atoi(&line[3]);
            job *j = find_job(pgid);
            continue_job(j, 1);
        }
        else if(strncmp(line, "bg", 2) == 0)
        {
            pid_t pgid = atoi(&line[3]);
            job *j = find_job(pgid);
            continue_job(j, 0);
        }
        else
            exec_sep(line);

        free(line);
    } while(1);
}


/*
 *  execute separators ; &
 */

static int exec_logic(char *str, int foreground);

static int
exec_sep(char *str)
{
    SepNode *curr = parse_sep(str);
    SepNode *head = curr;
    int res = 0;

    for(; curr; curr = curr->next)
        res = exec_logic(curr->cmd, (curr->sync) ? 1 : 0);

    free_sep_list(head);
    return res;
}

/*
 *  execute logic operators && ||
 */

static int exec_job(char *str, int foreground);

static int
exec_logic(char *str, int foreground)
{
    LogicNode *curr = parse_logic(str);
    LogicNode *head = curr;
    int last_status = 0;
    int skip = 0;

    for(; curr; curr = curr->next)
    {
        if(!skip)
            last_status = exec_job(curr->cmd, foreground);

        if(curr->type == LOGIC_AND)     skip = (last_status != 0);
        else if(curr->type == LOGIC_OR) skip = (last_status == 0);
        else                            skip = 0;
    }

    free_logic_list(head);
    return last_status;
}

static void update_environ(char *envp);

void
launch_process(process *p, pid_t pgid,
               int infile, int outfile, int errfile,
               int foreground)
{
    pid_t pid;

    if(shell_is_interactive)
    {
        pid = getpid();
        if(pgid == 0) pgid = pid;
        setpgid(pid, pgid);
        if(foreground)
            tcsetpgrp(shell_terminal, pgid);
    }

    if(infile != STDIN_FILENO)
    {
        dup2(infile, STDIN_FILENO);
        close(infile);
    }
    if(outfile != STDOUT_FILENO)
    {
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
    }
    if(errfile != STDERR_FILENO)
    {
        dup2(errfile, STDERR_FILENO);
        close(errfile);
    }

    redirection *r;

    for(r = p->redirs; r; r = r->next)
    {
        int fd;

        if(r->type == REDIR_FILE)
        {
            fd = open(r->filename, r->flags, 0644);
            if(fd < 0){
                perror("open");
                exit(1);
            }
            dup2(fd, r->fd_source);
            close(fd);
        }
        else if(r->type == REDIR_DUP)
        {
            dup2(atoi(r->filename), r->fd_source);
        }
        else // r->type == REDIR_CLOSE
        {
            close(r->fd_source);
        }
    }

    // already after fork
    for(int i = 0; p->envp[i]; i++)
        update_environ(p->envp[i]);
    
    sigprocmask(SIG_SETMASK, &prev_chld, NULL);
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);

    if(!p->argv[1] && p->argv[0][0] == '(') // subshell  .. | ( foo ...) | ..
    {
        shell_is_interactive = 0;
        char *sub_cmd = p->argv[0];
        int len = strlen(sub_cmd);

        if(sub_cmd[len - 1] == ')')
            sub_cmd[len - 1] = '\0';

        printf("subshell cmd: %s\n", &sub_cmd[1]);
        
        int res = exec_sep(&sub_cmd[1]); // skip opening parenthesis
        exit(res);
    }

    execvpe(p->argv[0], p->argv, my_environ.str);
    perror("execvp"); // should not reach here
    exit(1);
}

void
launch_job(job *j, int foreground)
{
    process *p;
    pid_t pid;
    int mypipe[2], infile, outfile;
    infile = j->stdin;

    sigprocmask(SIG_BLOCK, &mask_chld, &prev_chld);
    // add job
    j->next = first_job;
    first_job = j;
    
    for(p = j->first_process; p; p = p->next)
    {
        if(p->next)
        {
            if(pipe(mypipe) < 0)
            {
                perror("pipe");
                exit(1);
            }

            outfile = mypipe[1];
        }
        else
            outfile = j->stdout;
        
        pid = fork();
        if(pid == 0)
            launch_process(p, j->pgid, infile, outfile, j->stderr, foreground);
        else if(pid < 0)
        {
            perror("fork");
            exit(1);
        }
        else
        {
            p->pid = pid;
            if(shell_is_interactive)
            {
                if(!j->pgid)
                    j->pgid = pid;
                setpgid(pid, j->pgid);
            }
        }

        if(infile != j->stdin)   close(infile);
        if(outfile != j->stdout) close(outfile);
        infile = mypipe[0];
    }
    format_job_info(j, "launched");

    if(!shell_is_interactive)
        wait_for_job(j);
    else if(foreground)
        put_job_in_foreground(j, 0);
    else
        put_job_in_background(j, 0);

    sigprocmask(SIG_SETMASK, &prev_chld, NULL);
}

static void
update_environ(char *envp)
{
    char *var = malloc(sizeof(char) * 4096);
    int i;

    for(i = 0; envp[i] != '='; i++)
        var[i] = envp[i];
    var[i] = '=';
    var[i + 1] = '\0';
    
    for(int i = 0; my_environ.str[i]; i++)
        if(!strncmp(my_environ.str[i], var, strlen(var)))
        {
            free(my_environ.str[i]);
            my_environ.str[i] = strdup(envp);
            free(var);
            return;
        }
    
    // new var
    append_dyarray(&my_environ, envp);
    free(var);
}

static int
do_cd(char *target)
{
    if(!target)
        for(int i = 0; my_environ.str[i]; i++)
            if(!strncmp(my_environ.str[i], "HOME=", 5))
            {
                target = my_environ.str[i] + 5; // borrow FOO from HOME=FOO
                break;
            }

    int res = chdir(target);
    
    if(res < 0)
        perror("cd");

    char cwd[128];
    getcwd(cwd, sizeof(cwd));
    char new_pwd[128 + 5];
    snprintf(new_pwd, sizeof(new_pwd), "PWD=%s", cwd);
    update_environ(new_pwd);
    
    return res;
}

static int
exec_job(char *str, int foreground)
{
    job *j = parse_job(str);

    if(j->first_process->next == NULL) // not a pipeline
    {
        process *p = j->first_process;
        char *cmd = p->argv[0];

        if(cmd == NULL) // new env var
        {
            update_environ(j->first_process->envp[0]); // borrowing
            freejob(j);
            return 0;
        }

        if(strcmp(cmd, "cd") == 0)
        {
            do_cd(p->argv[1]);
            freejob(j);
            return 0;
        }
        else if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
        {
            cleanup_all();
            exit(0);
        }
    }

    launch_job(j, foreground);
    int status = j->status;
    last_exit_status = status;
    do_job_notification();
    return status;
}