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

void
sigchld_handler(int sig)
{
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    sigfillset(&mask_all);
    while((pid = waitpid(-1, &status, WNOHANG)) > 0){
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

        int is_fg = 0;

        for(int i = 0 ;i < MAX_JOBS; i++){
            if(jobs[i].pid == pid && jobs[i].state == FG){
                is_fg = 1;
                break;
            }
        }

        if(is_fg){
            fg_child_count--;
        }

        if(pid == last_pid && WIFEXITED(status)){
            last_status = WEXITSTATUS(status);
        }
        else{
            last_status = 1;
        }

        deletejob(pid);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    if(errno != ECHILD){
        perror("sigchld handler error\n");
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