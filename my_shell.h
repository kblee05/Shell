#ifndef MYSHELL_H
#define MYSHELL_H

#include <signal.h>
#include <sys/types.h>

extern volatile sig_atomic_t fg_child_count; // used for sigchld handler
extern volatile pid_t last_pid;
extern volatile int last_status;

void myshell_loop();

#endif