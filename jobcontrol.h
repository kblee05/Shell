#ifndef JOBCONTROL_H
#define JOBCONTROL_H

#include <sys/types.h>
#include <termios.h>

enum RedirType {
	REDIR_FILE,
	REDIR_DUP,
	REDIR_CLOSE,
	REDIR_NONE
};

struct redirection {
	struct redirection *next;
	enum RedirType type;
	char *filename;
	int fd_source;
	int flags;
};

struct process {
	struct process *next;
	char **argv;
	char **envp;
	pid_t pid;
	char completed;
	char stopped;
	int status;
	struct redirection *redirs;
};

struct job {
	struct job *next;
	char *command;
	struct process *first_process;
	pid_t pgid;
	char notified;
	struct termios tmodes;
	int stdin, stdout, stderr;
	int status;
};

extern struct job *first_job;

struct job *find_job(pid_t pgid);
void wait_for_job(struct job *j);
void put_job_in_foreground(struct job *j, int cont);
void put_job_in_background(struct job *j, int cont);
void update_status();
void do_job_notification();
void format_job_info(struct job *j, const char *status);
void freejob(struct job *j);
void continue_job(struct job *j, int foreground);
void cleanup_all();

struct job *new_job();
struct process *new_process();
struct redirection *new_redirection();

#endif