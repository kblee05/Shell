#include "sighandler.h"
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/*
 *  struct sigaction {
 *      void (*sa_handler)(int);                        >> These two are in union I suppose?
 *      void (*sa_sigaction)(int, siginfo_t *, void *); >> defined under __sigaction_handler
 *      sigset_t sa_mask;
 *      int sa_flags;
 *      void (*sa_restorer)(void);
 *  }
 */

typedef void (*sighandler_t)(int);

sighandler_t
*signal_wrapper(int signum, sighandler_t *handler){
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

    sigfillset(&mask_all);
    while((pid = waitpid(-1, NULL, 0)) > 0){
        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        // delete job
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }

    if(errno != ECHILD){
        write(2, "sigchld handler\n", 16);
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