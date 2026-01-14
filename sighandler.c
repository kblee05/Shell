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
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    update_status();
    sigprocmask(SIG_UNBLOCK, &prev_all, NULL);
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