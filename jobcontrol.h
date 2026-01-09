#ifndef JOBCONTROL_H
#define JOBCONTROL_H

#include <sys/types.h>

#define MAX_JOBS 16
#define MAXLINE 1024

#define UNDEF 0
#define FG 1
#define BG 2
#define ST 3

typedef struct job_t{
    pid_t pid;
    int jid;
    int state;
    char cmdline[MAXLINE];
}job_t;

extern job_t jobs[MAX_JOBS];

int addjob(pid_t pid, int state, char *cmdline);
int deletejob(pid_t pid);
int exec_jobctrl(char **args);

#endif