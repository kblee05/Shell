#ifndef SIGHANDLER_H
#define SIGHANDLER_H

typedef void (*sighandler_t)(int);

sighandler_t signal_wrapper(int signum, sighandler_t handler);
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

#endif