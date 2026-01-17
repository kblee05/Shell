#define _POSIX_C_SOURCE 200809L
#include "jobcontrol.h"
#include "my_shell.h"
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

job *first_job = NULL;

job *
find_job(pid_t pgid)
{
    job *j;

    for(j = first_job; j; j=j->next)
        if(j->pgid == pgid)
            return j;
    
    return NULL;
}

static int
job_is_stopped(job *j)
{
    process *p;

    for(p = j->first_process; p; p = p->next)
        if(!p->completed && !p->stopped)
            return 0;
    
    return 1;
}

static int
job_is_completed(job *j)
{
    process *p;

    for(p = j->first_process; p; p = p->next)
        if(!p->completed)
            return 0;
    
    return 1;
}

/* 
 * 
 *  SIGCHLD HAS TO BE BLOCKED BEFORE WAIT
 * 
 */

void wait_for_job(job *j);

void
put_job_in_foreground(job *j, int cont)
{
    tcsetpgrp(shell_terminal, j->pgid);

    if(cont)
    {
        tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
        if(kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
    }

    wait_for_job(j);

    tcsetpgrp(shell_terminal, shell_pgid);

    tcgetattr(shell_terminal, &j->tmodes);
    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

void
put_job_in_background(job *j, int cont)
{
    if(cont)
        if(kill(-j->pgid, SIGCONT) < 0)
            perror("kill (SIGCONT)");
}

static int
mark_process_status(pid_t pid,  int status)
{
    job *j;
    process *p;

    if(pid > 0)
    {
        for(j = first_job; j; j = j->next)
            for(p = j->first_process; p; p = p->next)
                if(p->pid == pid)
                {
                    p->status = status;
                    if(p->next == NULL)
                        j->status = status;

                    if(WIFSTOPPED(status))
                        p->stopped = 1;
                    else
                    {
                        p->completed = 1;
                        if(WIFSIGNALED(status))
                            fprintf(stderr, "%d: Terminated by signal %d.\n", (int) pid, WTERMSIG(p->status));
                    }
                    return 0;
                }
        fprintf(stderr, "No child process %d.\n", (int) pid);
        return -1;
    }
    else if(pid == 0 || errno == ECHILD)
        return -1;
    else
    {
        perror("waitpid");
        return -1;
    }
}

void
update_status()
{
    int status;
    pid_t pid;

    do
    {
        pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    } while (!mark_process_status(pid, status));
    
}

void
wait_for_job(job *j)
{
    int status;
    pid_t pid;

    do
    {
        sigsuspend(&prev_chld);
    } while (!job_is_stopped(j) &&
             !job_is_completed(j));
}

void
format_job_info(job *j, const char *status)
{
    fprintf(stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}

void
freejob(job *j)
{
    free(j->command);
    process *p = j->first_process;
    process *next;

    while(p)
    {
        next = p->next;

        redirection *r = p->redirs;
        redirection *next_r;

        while(r)
        {
            next_r = r->next;
            free(r->filename);
            free(r);
            r = next_r;
        }

        for(int i=0; p->argv[i] != 0; i++)
            free(p->argv[i]);
        free(p->argv);

        free(p);
        p = next;
    }

    free(j);
}

void
do_job_notification()
{
    job *j, *jlast, *jnext;

    sigprocmask(SIG_BLOCK, &mask_chld, &prev_chld);
    update_status();

    jlast = NULL;
    for(j = first_job; j; j = jnext)
    {
        jnext = j->next;

        if(job_is_completed(j))
        {
            format_job_info(j, "completed");
            if(jlast)
                jlast->next = jnext;
            else
                first_job = jnext;
            freejob(j);
        }
        else if(job_is_stopped(j) && !j->notified)
        {
            format_job_info(j, "stopped");
            j->notified = 1;
            jlast = j;
        }
        else
            jlast = j;
    }
    
    sigprocmask(SIG_SETMASK, &prev_chld, NULL);
}

static void
mark_job_as_running(job *j)
{
    process *p;

    for(p = j->first_process; p; p = p->next)
        p->stopped = 0;
    j->notified = 0;
}

void
continue_job(job *j, int foreground)
{
    mark_job_as_running(j);
    if(foreground)
        put_job_in_foreground(j, 1);
    else
        put_job_in_background(j, 1);
}

void
cleanup_all()
{
    job *j;
    job *next;
   
    for(j = first_job; j; j = next)
    {
        next = j-> next; // race condition due to sigchld handler(freejob)
        kill(-j->pgid, SIGCONT);
        kill(-j->pgid, SIGHUP);
    }
}