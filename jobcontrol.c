#define _POSIX_C_SOURCE 200809L
#include "jobcontrol.h"
#include "my_shell.h"
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int init = 0;
static int jid;
job_t jobs[MAX_JOBS];

static void
initjob()
{
    for(int i=0; i<MAX_JOBS; i++){
        jobs[i].pgid = 0;
    }
    jid = 0;
}

int
addjob(pid_t pgid, int state, char *cmdline)
{
    if(init == 0){
        initjob();
        init = 1;
    }

    if(pgid <= 0){
        return -1;
    }

    for(int i=0; i < MAX_JOBS; i++){
        if(jobs[i].pgid == 0){
            jobs[i].pgid = pgid;
            jobs[i].n_procs = 0;
            jobs[i].n_finished = 0;
            jobs[i].jid = jid++;
            jobs[i].state = state;
            strcpy(jobs[i].cmdline, cmdline);
            return jid - 1;
        }
    }
    // jobs is full
    return -1;
}

void
deletejob(job_t *job)
{
    job->pgid = 0;
    jid--;
}

int
exec_jobctrl(char **args)
{   
    sigset_t mask_all, prev_all;

    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    
    if(strcmp(args[0], "jobs") == 0){
        int no_jobs = 1;

        for(int i =0; i<MAX_JOBS; i++){
            if(jobs[i].pgid != 0){
                no_jobs = 0;
                printf("jid: %d, state: %d, cmd: %s\n", jobs[i].jid, jobs[i].state, jobs[i].cmdline);
            }
        }

        if(no_jobs){
            printf("No jobs running\n");
        }
        
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return 0;
    }
    else if(strcmp(args[0], "fg") == 0){
        int jid = atoi(args[1]);
        job_t *job;
        for(int i = 0; i<MAX_JOBS; i++){
            if(jobs[i].jid == jid){
                job = &jobs[i];
                break;
            }
        }
        kill(-job->pgid, SIGCONT);
        job->state = FG;    
        tcsetpgrp(0, job->pgid);
        fg_child_count = job->n_procs - job->n_finished;
        kill(-job->pgid, SIGCONT);

        while(fg_child_count > 0){
            sigsuspend(&prev_all);
        }

        tcsetpgrp(0, sh_pgid);
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return 0;
    }
    else if(strcmp(args[0], "bg") == 0){
        int jid = atoi(args[1]);
        job_t *job;
        for(int i = 0; i<MAX_JOBS; i++){
            if(jobs[i].jid == jid){
                job = &jobs[i];
                break;
            }
        }
        kill(-job->pgid, SIGCONT);
        job->state = BG;
        sigprocmask(SIG_SETMASK, &prev_all, NULL);
        return 0;
    }
    else if(strcmp(args[0], "disown") == 0){
        int jid = atoi(args[1]);
        job_t *job;
        for(int i = 0; i<MAX_JOBS; i++){
            if(jobs[i].jid == jid){
                job = &jobs[i];
                break;
            }
        }
        deletejob(job);
    }
    
    sigprocmask(SIG_SETMASK, &prev_all, NULL);
    return 1;
}