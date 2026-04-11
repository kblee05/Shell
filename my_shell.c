#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "my_shell.h"
#include "parser.h"
#include "dynamicstring.h"
#include "tokenizer.h"
#include "sighandler.h"
#include "jobcontrol.h"
#include "sighandler.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

// https://github.com/tokenrove/build-your-own-shell/blob/master/stage_1.md

pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
sigset_t mask_chld, prev_chld;
int last_exit_status = -1;

static struct dyarray my_environ;

extern char **environ;

char *get_environ(const char* key)
{
    	size_t len = strlen(key);

    	for(int i = 0; my_environ.str[i]; i++)
        	if(strncmp(my_environ.str[i], key, len) == 0 && 
		   my_environ.str[i][len] == '=')
            		return strdup(&my_environ.str[i][len + 1]);

   	return NULL;
}

int init_shell()
{
    	shell_terminal = STDERR_FILENO;
    	shell_is_interactive = isatty(shell_terminal);
    	my_environ = init_dyarray();
	
    	for(int i = 0; environ[i]; i++)
        	append_dyarray(&my_environ, environ[i]);

    	if (shell_is_interactive) {
        	while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            		kill(-shell_pgid, SIGTTIN);
        
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
		//signal(SIGCHLD, SIG_IGN);

		shell_pgid = getpid();

		if(setpgid(shell_pgid, shell_pgid) < 0) {
			perror("Couldn't put shell in it's own process group");
			return -1;
		}

		tcsetpgrp(shell_terminal, shell_pgid);
		tcgetattr(shell_terminal, &shell_tmodes);

		signal_wrapper(SIGCHLD, sigchld_handler);
		sigemptyset(&mask_chld);
		sigaddset(&mask_chld, SIGCHLD);
		//sigprocmask(SIG_UNBLOCK, &mask_chld, NULL);
    	}

	return 0;
}

/*
 *  Start of the shell
 */

static int exec_sep(char *str);

int myshell_loop()
{
	if (init_shell() != 0)
		return -1;

    	while(1) {
        	do_job_notification();
        	printf("< ");
        	
		char *line;
        	
		while (!((line) = readline())) {}

        	if (strncmp(line, "jobs", 4) == 0) {
            		for(struct job *j = first_job; j; j = j->next)
                		format_job_info(j, (j->first_process->stopped) ? "stopped" : "running");
        	} 
		else if (strncmp(line, "fg", 2) == 0) {
            		pid_t pgid = atoi(&line[3]);
            		struct job *j = find_job(pgid);
            		continue_job(j, 1);
        	}
		else if(strncmp(line, "bg", 2) == 0) {
            		pid_t pgid = atoi(&line[3]);
            		struct job *j = find_job(pgid);
            		continue_job(j, 0);
        	}
        	else
            		exec_sep(line);
		
        	free(line);
    	}
}


/*
 *  execute separators ; &
 */

static int exec_logic(char *str, int foreground);

static int exec_sep(char *str)
{
    	struct SepNode *curr = parse_sep(str);
    	struct SepNode *head = curr;
    	int res = 0;

    	for(; curr; curr = curr->next)
        	res = exec_logic(curr->cmd, (curr->sync) ? 1 : 0);

    	free_sep_list(head);
    	return res;
}

/*
 *  execute logic operators && ||
 */

static int exec_job(char *str, int foreground);

static int exec_logic(char *str, int foreground)
{
    	struct LogicNode *curr = parse_logic(str);
    	struct LogicNode *head = curr;
    	int last_status = 0;
    	int skip = 0;

    	for(; curr; curr = curr->next) {
        	if(!skip)
            		last_status = exec_job(curr->cmd, foreground);

        	switch (curr->type) {
		case LOGIC_AND:  skip = (last_status != 0); break;
		case LOGIC_OR:   skip = (last_status == 0); break;
		case LOGIC_NONE: skip = 0;                  break;
		// should not reach here
		default:         skip = 0;                  break;
		}
    	}

    	free_logic_list(head);
    	
	return last_status;
}

static void update_environ(const char *envp);

void launch_process(struct process *p, pid_t pgid,
                    int infile, int outfile, int errfile,
                    int foreground)
{
    	pid_t pid;

    	if(shell_is_interactive) {
        	pid = getpid();
        	
		if(pgid == 0)
			pgid = pid;
        	
		setpgid(pid, pgid);
        
		if(foreground)
            		tcsetpgrp(shell_terminal, pgid);
    	}

	if(infile != STDIN_FILENO) {
		dup2(infile, STDIN_FILENO);
		close(infile);
	}
	if(outfile != STDOUT_FILENO) {
		dup2(outfile, STDOUT_FILENO);
		close(outfile);
	}
	if(errfile != STDERR_FILENO) {
		dup2(errfile, STDERR_FILENO);
		close(errfile);
	}

    	struct redirection *r;

    	for(r = p->redirs; r; r = r->next) {
		switch (r->type) {
		case REDIR_FILE:
        		int fd;
			if ((fd = open(r->filename, r->flags, 0644) < 0)) {
				fprintf(stderr, "error open");
				return;
			}

			dup2(fd, r->fd_source);
			close(fd);
			break;
		case REDIR_DUP:
			dup2(atoi(r->filename), r->fd_source);
			break;
		case REDIR_CLOSE:
			close(r->fd_source);
			break;
		case REDIR_NONE:
			break;
		}
    	}

    	// already after fork
    	for(int i = 0; p->envp[i]; i++)
        	update_environ(p->envp[i]);
    
	sigprocmask(SIG_SETMASK, &prev_chld, NULL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);

	// subshell  .. | ( foo ...) | ..
    	if(p->argv[1] == NULL && p->argv[0][0] == '(') {
		shell_is_interactive = 0;
		char *sub_cmd = &p->argv[0][1];
		int len = strlen(sub_cmd);

		if(sub_cmd[len - 1] == ')')
			sub_cmd[len - 1] = '\0';

		printf("subshell cmd: %s\n", sub_cmd);
		
		int res = exec_sep(&sub_cmd);
		exit(res);
    	}

	execvpe(p->argv[0], p->argv, my_environ.str);
	
	fprintf(stderr, "execvp"); // should not reach here
	exit(1);
}

void launch_job(struct job *j, int foreground)
{
	struct process *p;
	pid_t pid;
	int mypipe[2], infile, outfile;
	infile = j->stdin;

	sigprocmask(SIG_BLOCK, &mask_chld, &prev_chld);
	// add job
	j->next = first_job;
	first_job = j;
	
	for (p = j->first_process; p; p = p->next) {
		if (p->next) {
			if(pipe(mypipe) < 0) {
				fprintf(stderr, "pipe");
				exit(1);
			}

			outfile = mypipe[1];
		}
		else
			outfile = j->stdout;
		
		pid = fork();
		if (pid == 0)
			launch_process(p, j->pgid, infile, outfile, j->stderr, foreground);
		else if (pid < 0) {
			fprintf(stderr, "fork");
			exit(1);
		}
		else {
			p->pid = pid;
			
			if (shell_is_interactive) {
				if (!j->pgid)
					j->pgid = pid;
				
				setpgid(pid, j->pgid);
			}
		}

		if(infile != j->stdin)   close(infile);
		if(outfile != j->stdout) close(outfile);
		infile = mypipe[0];
	}

	format_job_info(j, "launched");

	if (!shell_is_interactive)
		wait_for_job(j);
	else if (foreground)
		put_job_in_foreground(j, 0);
	else
		put_job_in_background(j, 0);
	
	sigprocmask(SIG_SETMASK, &prev_chld, NULL);
}

#define MAX_VAR_LEN 4096

static void update_environ(const char *envp)
{
	char *var = malloc(sizeof(char) * MAX_VAR_LEN);
	int i;

	for (i = 0; envp[i] != '='; i++)
		var[i] = envp[i];
	var[i] = '=';
	var[i + 1] = '\0';
	
	for (int i = 0; my_environ.str[i]; i++)
		if(strncmp(my_environ.str[i], var, strlen(var)) == 0) {
			free(my_environ.str[i]);
			my_environ.str[i] = strdup(envp);
			free(var);
			return;
		}
	
	// new var
	append_dyarray(&my_environ, envp);
	free(var);
}

static int do_cd(char *target)
{
	if (target == NULL)
		for(int i = 0; my_environ.str[i]; i++)
			if(strncmp(my_environ.str[i], "HOME=", 5) == 0) {
				target = my_environ.str[i] + 5; // borrow FOO from HOME=FOO
				break;
			}
	
	int chdir_ret;
	if ((chdir_ret = chdir(target)) < 0)
		fprintf(stderr, "cd");

	char cwd[128];
	getcwd(cwd, sizeof(cwd));

	char new_pwd[128 + 5];
	snprintf(new_pwd, sizeof(new_pwd), "PWD=%s", cwd);
	
	update_environ(new_pwd);
	
	return chdir_ret;
}

static int exec_job(char *str, int foreground)
{
    	struct job *j = parse_job(str);

    	if (j->first_process->next == NULL) {
        	struct process *p = j->first_process;
        	char *cmd = p->argv[0];

		// new env var
        	if (cmd == NULL) {
            		update_environ(j->first_process->envp[0]);
			freejob(j);
			return 0;
		}

        	if(strcmp(cmd, "cd") == 0) {
			do_cd(p->argv[1]);
			freejob(j);
			return 0;
        	}
        	else if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
		{
			cleanup_all();
			exit(0);
		}
    	}

	launch_job(j, foreground);
	int status = j->status;
	last_exit_status = status;
	do_job_notification();

	return status;
}