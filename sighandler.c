#define _POSIX_C_SOURCE 200809L
#include "sighandler.h"
#include "my_shell.h"
#include "jobcontrol.h"
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdlib.h>

/*
 *  struct sigaction {
 *      void (*sa_handler)(int);                        >> These two are in union I suppose?
 *      void (*sa_sigaction)(int, siginfo_t *, void *); >> defined under __sigaction_handler
 *      sigset_t sa_mask;
 *      int sa_flags;
 *      void (*sa_restorer)(void);
 *  }
 */

sighandler_t
signal_wrapper(int signum, sighandler_t handler){
    struct sigaction action, old_action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); // signum is automatically blocked, no need to block it manually
    action.sa_flags = SA_RESTART;

    if(sigaction(signum, &action, &old_action) == -1){
        perror("sigaction error");
        exit(1);
    }

    return old_action.sa_handler;
}

static int
get_job_idx(pid_t pid)
{
    for(int i=0; i<MAX_JOBS; i++){
        for(int j=0; j<jobs[i].n_procs; j++){
            if(jobs[i].pids[j] == pid)
                return i;
        }
    }

    return -1;
}

void
sigchld_handler(int sig)
{
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    sigfillset(&mask_all);
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

        int j_idx = get_job_idx(pid);
        if(j_idx == -1) continue;
        job_t *job = &jobs[j_idx];

        if(WIFEXITED(status) || WIFSIGNALED(status)){
            job->n_finished++;
        }

        if(WIFSTOPPED(status)){
            job->state = ST;
        }

        if(job->state == FG || WIFSTOPPED(status)){
            fg_child_count--;
        }

        if(job->n_procs == job->n_finished){
            deletejob(job);
        }

        if(pid == last_pid && WIFEXITED(status)){
            last_status = WEXITSTATUS(status);
        }
        else{
            last_status = 1;
        }

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    if(pid == - 1 && errno != ECHILD){
        fprintf(stderr, "sigchld handler error\n");
    }
    errno = olderrno;
}

void
sigint_handler(int sig)
{

}

void
sigtstp_handler(int sig)
{

}