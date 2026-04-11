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

struct job *first_job = NULL;

struct job *find_job(pid_t pgid)
{
	for(struct job *j = first_job; j; j = j->next)
		if(j->pgid == pgid)
			return j;

	return NULL;
}

static int job_is_stopped(const struct job *j)
{
	for(struct process *p = j->first_process; p; p = p->next)
		if(!p->completed && !p->stopped)
			return 0;

	return 1;
}

static int job_is_completed(const struct job *j)
{
	for(struct process *p = j->first_process; p; p = p->next)
		if(!p->completed)
			return 0;

	return 1;
}

/* 
 * 
 *  SIGCHLD HAS TO BE BLOCKED BEFORE WAIT
 * 
 */

void put_job_in_foreground(struct job *j, int cont)
{
	tcsetpgrp(shell_terminal, j->pgid);

	if (cont) {
		tcsetattr(shell_terminal, TCSADRAIN, &j->tmodes);
		
		if(kill(-j->pgid, SIGCONT) < 0)
			fprintf(stderr, "kill (SIGCONT)");
	}

	wait_for_job(j);

	tcsetpgrp(shell_terminal, shell_pgid);
	tcgetattr(shell_terminal, &j->tmodes);
	tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

void put_job_in_background(struct job *j, int cont)
{
	if(cont && kill(-j->pgid, SIGCONT) < 0)
		fprintf(stderr, "kill (SIGCONT)");
}
/*
 *  return 0 and success
 *  return -1 if error
 */
static int mark_process_status(pid_t pid,  int status)
{
	if (pid == 0 || (pid == -1 && errno == ECHILD))
		return -1;
	
	if (pid < 0) {
		fprintf(stderr, "waitpid");
		return -1;
	}
	
	struct process *p;
	struct job *j;
	
	for(j = first_job; j; j = j->next)
		for(p = j->first_process; p; p = p->next)
			if (p->pid == pid) {
				p->status = status;
			
				if (p->next == NULL)
					j->status = status;

				if (WIFSTOPPED(status))
					p->stopped = 1;
				else {
					p->completed = 1;
					
					if (WIFSIGNALED(status))
						fprintf(stderr, "%d: Terminated by signal %d.\n", (int) pid, WTERMSIG(p->status));
				}
				
				return 0;
			}

	fprintf(stderr, "No child process %d.\n", (int) pid);
	return -1;
}

void update_status()
{
	int status;
	pid_t pid;

	do {
		pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
	} while (!mark_process_status(pid, status));
}

void wait_for_job(struct job *j)
{
	do {
		sigsuspend(&prev_chld);
	} while (!job_is_stopped(j) && !job_is_completed(j));
}

void format_job_info(struct job *j, const char *status)
{
    	fprintf(stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->command);
}

void freejob(struct job *j)
{
	struct process *p;
	struct process *next_p;

	for (p = j->first_process; p; p = next_p) {
		next_p = p->next;

		struct redirection *r;
		struct redirection *next_r;

		for (r = p->redirs; r; r = next_r) {
			next_r = r->next;

			free(r->filename);
		}
		free(r);

		for (int i = 0; p->argv[i]; i++) {
			free(p->argv[i]);
		}
		free(p->argv);

		for (int i = 0; p->envp[i]; i++) {
			free(p->envp[i]);
		}
		free(p->envp);

		free(p);
	}

	free(j->command);
	free(j);
}

void do_job_notification()
{
	struct job *j, *jlast, *jnext;

	sigprocmask(SIG_BLOCK, &mask_chld, &prev_chld);
	update_status();

	jlast = NULL;

	for (j = first_job; j; j = jnext) {
		jnext = j->next;

		if (job_is_completed(j)) {
			format_job_info(j, "completed");
			
			if (jlast)
				jlast->next = jnext;
			else
				first_job = jnext;
			
			freejob(j);
		} else if (job_is_stopped(j) && !j->notified) {
			format_job_info(j, "stopped");
			j->notified = 1;
			jlast = j;
		} else
			jlast = j;
	}

	sigprocmask(SIG_SETMASK, &prev_chld, NULL);
}

static void mark_job_as_running(struct job *j)
{
	for(struct process *p = j->first_process; p; p = p->next)
		p->stopped = 0;
	j->notified = 0;
}

void continue_job(struct job *j, int foreground)
{
    	mark_job_as_running(j);
    
	if (foreground)
        	put_job_in_foreground(j, 1);
    	else
        	put_job_in_background(j, 1);
}

void cleanup_all()
{
	sigset_t mask_clean, prev_clean;
	sigemptyset(&mask_clean);
	sigaddset(&mask_clean, SIGCHLD);
	sigprocmask(SIG_BLOCK ,&mask_clean, &prev_clean);

	struct job *j;
	struct job *next;

	for(j = first_job; j; j = next) {
		// potential race condition due to sigchld handler(freejob)
		// need to block SIGCHLD
		next = j-> next;
		kill(-j->pgid, SIGCONT);
		kill(-j->pgid, SIGHUP);
	}

	sigprocmask(SIG_SETMASK, &prev_clean, NULL);
}

struct job *new_job() {
    	struct job *j = malloc(sizeof(struct job));
	*j = (struct job) {
		.next          = NULL,
		.command       = NULL,
		.first_process = NULL,
		j->pgid        = 0, // default for non interactive shell
		j->notified    = 0,
		j->tmodes      = shell_tmodes,
		j->stdin       = STDIN_FILENO,
		j->stdout      = STDOUT_FILENO,
		j->stderr      = STDERR_FILENO,
		j->status      = -1
	};

	return j;
}

#define MIN_ARG_COUNT 32

struct process *new_process()
{
    	struct process *p = malloc(sizeof(struct process));
	*p = (struct process) {
		.argv      = malloc(sizeof(char *) * MIN_ARG_COUNT),
		.next      = NULL,
		.pid       = -1,
		.completed = 0,
		.stopped   = 0,
		.status    = -1,
		.redirs    = NULL,
		.envp      = NULL
	};

	return p;
}

struct redirection *new_redirection() {
	struct redirection *r = malloc(sizeof(struct redirection));
	*r = (struct redirection) {
		.fd_source = -1,
		.next      = NULL,
		.type      = REDIR_NONE,
		.filename  = NULL,
		.flags     = 0
	};

	return r;
}