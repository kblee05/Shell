#ifndef MYSHELL_H
#define MYSHELL_H

#include <signal.h>
#include <sys/types.h>
#include "jobcontrol.h"
#include "dynamicstring.h"

extern pid_t shell_pgid;
extern struct termios shell_tmodes;
extern int shell_terminal;
extern int shell_is_interactive;
extern sigset_t mask_chld, prev_chld;
extern int last_exit_status;

void myshell_loop();
char *get_environ(char* key);

#endif