#ifndef JOBCONTROL_H
#define JOBCONTROL_H

#include <sys/types.h>
#include <termios.h>

typedef struct process
{
    struct process *next;
    char **argv;
    pid_t pid;
    char completed;
    char stopped;
    int status;
} process;

typedef struct job
{
    struct job *next;
    char *command;
    process *first_process;
    pid_t pgid;
    char notified;
    struct termios tmodes;
    int stdin, stdout, stderr;
    int status;
} job;

extern job *first_job;

job *find_job(pid_t pgid);
void wait_for_job(job *j);
void put_job_in_foreground(job *j, int cont);
void put_job_in_background(job *j, int cont);
void update_status();
void do_job_notification();
void format_job_info(job *j, const char *status);
void freejob(job *j);
void continue_job(job *j, int foreground);

#endif