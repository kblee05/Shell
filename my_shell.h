#ifndef MYSHELL_H
#define MYSHELL_H

char *myshell_readline();
char **myshell_parseline(char* line);
int myshell_execute(char **args);
void myshell_loop();

#endif