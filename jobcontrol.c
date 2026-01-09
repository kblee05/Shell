#define _POSIX_C_SOURCE 200809L
#include "jobcontrol.h"
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

static int init = 0;
static int jid;
job_t jobs[MAX_JOBS];

static void
initjob()
{
    for(int i=0; i<MAX_JOBS; i++){
        jobs[i].pid = 0;
    }
    jid = 0;
}

int
addjob(pid_t pid, int state, char *cmdline)
{
    if(init == 0){
        initjob();
        init = 1;
    }

    if(pid <= 0){
        return 0;
    }

    for(int i=0; i < MAX_JOBS; i++){
        if(jobs[i].pid == 0){
            jobs[i].pid = pid;
            jobs[i].jid = jid++;
            jobs[i].state = state;
            strcpy(jobs[i].cmdline, cmdline);
            return 0;
        }
    }
    // jobs is full
    return 1;
}

int
deletejob(pid_t pid)
{
    if(pid <= 0){
        return 0;
    }

    for(int i=0; i<MAX_JOBS; i++){
        if(jobs[i].pid == pid){
            jobs[i].pid = 0;
            jobs[i].jid = 0;
            jobs[i].state = UNDEF;
            jobs[i].cmdline[0] = '\0';
            return 0;
        }
    }

    return 1;
}

int
exec_jobctrl(char **args)
{   
    sigset_t mask_all, prev_all;

    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    
    if(strcmp(args[0], "jobs") == 0){
        for(int i =0; i<MAX_JOBS; i++){
            if(jobs[i].pid != 0)
                printf("jid: %d, state: %d, cmd: %s\n", jobs[i].jid, jobs[i].state, jobs[i].cmdline);
        }
        
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return 0;
    }
    else if(strcmp(args[0], "fg") == 0){

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return 0;
    }
    else if(strcmp(args[0], "bg") == 0){

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return 0;
    }
    
    sigprocmask(SIG_SETMASK, &prev_all, NULL);
    return 1;
}