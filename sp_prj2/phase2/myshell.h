#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAXLINE 8192
#define MAXARGS 128
#define MAXCMDS 16

void eval(char *cmdline);
void unix_error(const char *msg);
int builtin_command(char **argv);

pid_t Fork(void);
void Execvp(const char *file, char *const argv[]);
pid_t Waitpid(pid_t pid, int *iptr, int options);
int Pipe(int fd[2]);
int parseline(char *buf, char **argv);
void run_pipe(char **cmds, int num_cmds);

#endif
