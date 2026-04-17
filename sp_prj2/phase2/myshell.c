#include "myshell.h"

static char *find_pipe(char *buf);
static int parse_command(char *cmd, char **argv);
static pid_t run_pipe(char *cmdline, int in_fd, pid_t *pids, int *nchild);

int main(void) {
    char cmdline[MAXLINE];

    while (1) {
        printf("CSE4100-SP-P2> ");
        fflush(stdout);

        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) exit(0);
            continue;
        }

        eval(cmdline);
    }
}

void eval(char *cmdline) {
    char buf[MAXLINE];
    char *argv[MAXARGS];
    pid_t pid;

    strcpy(buf, cmdline);
    buf[strcspn(buf, "\n")] = '\0';

    if (buf[0] == '\0') return;

    if (find_pipe(buf) != NULL) {
        pid_t pids[MAXCMDS];
        int nchild = 0;

        run_pipe(buf, STDIN_FILENO, pids, &nchild);

        for (int i = 0; i < nchild; i++) {
            Waitpid(pids[i], NULL, 0);
        }
        return;
    }

    parse_command(buf, argv);

    if (argv[0] == NULL) return;

    if (!builtin_command(argv)) {
        pid = Fork();
        if (pid == 0) {
            Execvp(argv[0], argv);
            exit(1);
        }
        Waitpid(pid, NULL, 0);
    }
}

void unix_error(const char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

int builtin_command(char **argv) {
    if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else if (chdir(argv[1]) < 0) {
            perror("cd error");
        }
        return 1;
    }

    if (!strcmp(argv[0], "exit")) {
        exit(0);
    }

    return 0;
}

pid_t Fork(void) {
    pid_t pid;
    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}

void Execvp(const char *file, char *const argv[]) {
    if (execvp(file, argv) < 0)
        unix_error("Execvp error");
}

pid_t Waitpid(pid_t pid, int *iptr, int options) {
    pid_t retpid;
    if ((retpid = waitpid(pid, iptr, options)) < 0)
        unix_error("Waitpid error");
    return retpid;
}

int Pipe(int fd[2]) {
    if (pipe(fd) < 0) {
        unix_error("Pipe error");
        return -1;
    }
    return 0;
}

static int parse_command(char *cmd, char **argv) {
    int argc = 0;
    char *p = cmd;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;

        if (*p == '\0')
            break;

        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            argv[argc++] = p;

            while (*p != '\0' && *p != quote)
                p++;

            if (*p == quote) {
                *p = '\0';
                p++;
            }
        } else {
            argv[argc++] = p;

            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n')
                p++;

            if (*p != '\0') {
                *p = '\0';
                p++;
            }
        }

        if (argc >= MAXARGS - 1)
            break;
    }

    argv[argc] = NULL;
    return argc;
}

static char *find_pipe(char *buf) {
    char *p = buf;
    char quote = 0;

    while (*p != '\0') {
        if (quote == 0 && (*p == '"' || *p == '\'')) {
            quote = *p;
        } else if (quote != 0 && *p == quote) {
            quote = 0;
        } else if (quote == 0 && *p == '|') {
            return p;
        }
        p++;
    }
    return NULL;
}

static pid_t run_pipe(char *cmdline, int in_fd, pid_t *pids, int *nchild) {
    char *pipe_pos = find_pipe(cmdline);
    char *argv[MAXARGS];
    pid_t pid;
    int fd[2];

    if (pipe_pos == NULL) {
        parse_command(cmdline, argv);
        if (argv[0] == NULL)
            return -1;

        pid = Fork();
        if (pid == 0) {
            if (in_fd != STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            Execvp(argv[0], argv);
            exit(1);
        }

        pids[(*nchild)++] = pid;

        if (in_fd != STDIN_FILENO)
            close(in_fd);

        return pid;
    } 

    *pipe_pos = '\0';
    char *left = cmdline;
    char *right = pipe_pos + 1;

    while (*right == ' ' || *right == '\t')
        right++;

    parse_command(left, argv);
    if (argv[0] == NULL) {
        if (in_fd != STDIN_FILENO)
            close(in_fd);
        return run_pipe(right, STDIN_FILENO, pids, nchild);
    }

    Pipe(fd);

    pid = Fork();
    if (pid == 0) {
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }

        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);

        Execvp(argv[0], argv);
        exit(1);
    }

    pids[(*nchild)++] = pid;

    if (in_fd != STDIN_FILENO)
        close(in_fd);

    close(fd[1]);

    return run_pipe(right, fd[0], pids, nchild);
}
