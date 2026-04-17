#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXLINE 8192
#define MAXARGS 128
#define MAXCMDS 16
#define MAXJOBS 16

typedef enum {
    JOB_EMPTY = 0,
    JOB_FOREGROUND,
    JOB_BACKGROUND,
    JOB_STOPPED
} job_state_t;

typedef struct {
    int used;
    int jid;
    pid_t pgid;
    pid_t pids[MAXCMDS];
    int npids;
    int live_count;
    job_state_t state;
    char cmdline[MAXLINE];
} job_t;

void eval(char *cmdline);
void unix_error(const char *msg);
int builtin_command(char **argv);

pid_t Fork(void);
void Execvp(const char *file, char *const argv[]);
pid_t Waitpid(pid_t pid, int *iptr, int options);
int Pipe(int fd[2]);

#endif
