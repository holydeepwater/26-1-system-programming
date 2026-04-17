#include "myshell.h"

static job_t jobs[MAXJOBS];
static int next_jid = 1;
static volatile sig_atomic_t fg_pgid = 0;

static void init_jobs(void);
static void install_signal_handlers(void);
static void sigchld_handler(int sig);
static void sigint_handler(int sig);
static void sigtstp_handler(int sig);

static char *find_pipe(char *buf);
static int parse_command(char *cmd, char **argv);
static int split_pipeline(char *cmdline, char **segments);
static int parse_line(char *buf, char **segments, int *bg);
static void normalize_jobcmd(char *jobcmd);
static int launch_pipeline(char **segments, int nseg, int bg, const char *cmdline);

static sigset_t chld_mask(void);
static void block_sigchld(sigset_t *prev);
static void restore_sigmask(const sigset_t *prev);

static job_t *add_job(pid_t pgid, pid_t *pids, int npids, job_state_t state,
                      const char *cmdline);
static void delete_job(job_t *job);
static job_t *find_job_by_jid(int jid);
static job_t *find_job_by_pid(pid_t pid);
static job_t *find_foreground_job(void);
static void update_fg_pgid(void);
static const char *job_state_name(job_state_t state);
static void list_jobs(void);
static int parse_job_id(const char *arg, int *jid);
static int builtin_bg(char **argv);
static int builtin_fg(char **argv);
static int builtin_kill(char **argv);
static void wait_for_foreground(pid_t pgid);

int main(void) {
    char cmdline[MAXLINE];

    init_jobs();
    install_signal_handlers();

    while (1) {
        printf("CSE4100-SP-P3> ");
        fflush(stdout);

        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) {
                putchar('\n');
                exit(0);
            }

            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }

            continue;
        }

        eval(cmdline);
    }
}

void eval(char *cmdline) {
    char buf[MAXLINE];
    char jobcmd[MAXLINE];
    char *segments[MAXCMDS];
    int bg = 0;
    int nseg;

    strcpy(buf, cmdline);
    buf[strcspn(buf, "\n")] = '\0';

    if (buf[0] == '\0') {
        return;
    }

    strncpy(jobcmd, buf, MAXLINE - 1);
    jobcmd[MAXLINE - 1] = '\0';
    normalize_jobcmd(jobcmd);

    nseg = parse_line(buf, segments, &bg);
    if (nseg <= 0) {
        return;
    }

    if (nseg == 1) {
        char builtin_buf[MAXLINE];
        char *argv[MAXARGS];

        strncpy(builtin_buf, segments[0], MAXLINE - 1);
        builtin_buf[MAXLINE - 1] = '\0';
        parse_command(builtin_buf, argv);
        if (argv[0] == NULL) {
            return;
        }

        if (builtin_command(argv)) {
            return;
        }
    }

    launch_pipeline(segments, nseg, bg, jobcmd);
}

void unix_error(const char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

int builtin_command(char **argv) {
    if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else if (chdir(argv[1]) < 0) {
            perror("cd");
        }
        return 1;
    }

    if (!strcmp(argv[0], "exit")) {
        exit(0);
    }

    if (!strcmp(argv[0], "jobs")) {
        list_jobs();
        return 1;
    }

    if (!strcmp(argv[0], "bg")) {
        return builtin_bg(argv);
    }

    if (!strcmp(argv[0], "fg")) {
        return builtin_fg(argv);
    }

    if (!strcmp(argv[0], "kill")) {
        return builtin_kill(argv);
    }

    return 0;
}

pid_t Fork(void) {
    pid_t pid;

    if ((pid = fork()) < 0) {
        unix_error("fork");
    }

    return pid;
}

void Execvp(const char *file, char *const argv[]) {
    if (execvp(file, argv) < 0) {
        fprintf(stderr, "%s: Command not found\n", file);
        _exit(1);
    }
}

pid_t Waitpid(pid_t pid, int *iptr, int options) {
    pid_t retpid;

    if ((retpid = waitpid(pid, iptr, options)) < 0 && errno != ECHILD) {
        unix_error("waitpid");
    }

    return retpid;
}

int Pipe(int fd[2]) {
    if (pipe(fd) < 0) {
        unix_error("pipe");
        return -1;
    }

    return 0;
}

static void init_jobs(void) {
    memset(jobs, 0, sizeof(jobs));
}

static void install_signal_handlers(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    signal(SIGQUIT, SIG_IGN);
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

static int parse_command(char *cmd, char **argv) {
    int argc = 0;
    char *p = cmd;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[argc++] = p;

            while (*p != '\0' && *p != quote) {
                p++;
            }
        } else {
            argv[argc++] = p;

            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
                p++;
            }
        }

        if (*p != '\0') {
            *p++ = '\0';
        }

        if (argc >= MAXARGS - 1) {
            break;
        }
    }

    argv[argc] = NULL;
    return argc;
}

static int split_pipeline(char *cmdline, char **segments) {
    int nseg = 0;
    char *start = cmdline;
    char *pipe_pos;

    while (start != NULL && *start != '\0' && nseg < MAXCMDS) {
        while (*start == ' ' || *start == '\t') {
            start++;
        }

        pipe_pos = find_pipe(start);
        if (pipe_pos != NULL) {
            *pipe_pos = '\0';
        }

        segments[nseg++] = start;
        start = (pipe_pos == NULL) ? NULL : pipe_pos + 1;
    }

    return nseg;
}

static int parse_line(char *buf, char **segments, int *bg) {
    char quote = 0;
    char *amp = NULL;
    char *p;

    *bg = 0;

    for (p = buf; *p != '\0'; p++) {
        if (quote == 0 && (*p == '"' || *p == '\'')) {
            quote = *p;
        } else if (quote != 0 && *p == quote) {
            quote = 0;
        } else if (quote == 0 && *p == '&') {
            amp = p;
        }
    }

    if (amp != NULL) {
        char *tail = amp + 1;

        while (*tail == ' ' || *tail == '\t') {
            tail++;
        }

        if (*tail == '\0') {
            *bg = 1;
            *amp = '\0';
        }
    }

    if (buf[0] != '\0') {
        for (p = buf + strlen(buf) - 1; p >= buf && (*p == ' ' || *p == '\t'); p--) {
            *p = '\0';
        }
    }

    if (buf[0] == '\0') {
        return 0;
    }

    return split_pipeline(buf, segments);
}

static void normalize_jobcmd(char *jobcmd) {
    char *p;

    if (jobcmd[0] != '\0') {
        for (p = jobcmd + strlen(jobcmd) - 1;
             p >= jobcmd && (*p == ' ' || *p == '\t');
             p--) {
            *p = '\0';
        }
    }

    if (jobcmd[0] != '\0') {
        p = jobcmd + strlen(jobcmd) - 1;
        if (*p == '&') {
            *p = '\0';
        }
    }

    if (jobcmd[0] != '\0') {
        for (p = jobcmd + strlen(jobcmd) - 1;
             p >= jobcmd && (*p == ' ' || *p == '\t');
             p--) {
            *p = '\0';
        }
    }
}

static int launch_pipeline(char **segments, int nseg, int bg, const char *cmdline) {
    int i;
    int in_fd = STDIN_FILENO;
    int fd[2] = {-1, -1};
    pid_t pids[MAXCMDS];
    pid_t pgid = 0;
    sigset_t prev;
    job_t *job;

    block_sigchld(&prev);

    for (i = 0; i < nseg; i++) {
        char *argv[MAXARGS];
        pid_t pid;

        parse_command(segments[i], argv);
        if (argv[0] == NULL) {
            continue;
        }

        if (i < nseg - 1 && Pipe(fd) < 0) {
            restore_sigmask(&prev);
            return -1;
        }

        pid = Fork();
        if (pid == 0) {
            restore_sigmask(&prev);

            if (pgid == 0) {
                pgid = getpid();
            }
            setpgid(0, pgid);

            if (in_fd != STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if (i < nseg - 1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            Execvp(argv[0], argv);
        }

        if (pgid == 0) {
            pgid = pid;
        }
        setpgid(pid, pgid);
        pids[i] = pid;

        if (in_fd != STDIN_FILENO) {
            close(in_fd);
        }

        if (i < nseg - 1) {
            close(fd[1]);
            in_fd = fd[0];
        }
    }

    if (in_fd != STDIN_FILENO) {
        close(in_fd);
    }

    job = add_job(pgid, pids, nseg, bg ? JOB_BACKGROUND : JOB_FOREGROUND, cmdline);
    restore_sigmask(&prev);

    if (job == NULL) {
        fprintf(stderr, "job table is full\n");
        kill(-pgid, SIGKILL);
        return -1;
    }

    if (bg) {
        printf("[%d] %d %s\n", job->jid, job->pgid, job->cmdline);
    } else {
        wait_for_foreground(pgid);
    }

    return 0;
}

static sigset_t chld_mask(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    return mask;
}

static void block_sigchld(sigset_t *prev) {
    sigset_t mask = chld_mask();
    sigprocmask(SIG_BLOCK, &mask, prev);
}

static void restore_sigmask(const sigset_t *prev) {
    sigprocmask(SIG_SETMASK, prev, NULL);
}

static job_t *add_job(pid_t pgid, pid_t *pids, int npids, job_state_t state,
                      const char *cmdline) {
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used) {
            jobs[i].used = 1;
            jobs[i].jid = next_jid++;
            jobs[i].pgid = pgid;
            jobs[i].npids = npids;
            jobs[i].live_count = npids;
            jobs[i].state = state;
            strncpy(jobs[i].cmdline, cmdline, MAXLINE - 1);
            jobs[i].cmdline[MAXLINE - 1] = '\0';
            memcpy(jobs[i].pids, pids, sizeof(pid_t) * npids);
            update_fg_pgid();
            return &jobs[i];
        }
    }

    return NULL;
}

static void delete_job(job_t *job) {
    if (job == NULL) {
        return;
    }

    memset(job, 0, sizeof(*job));
    update_fg_pgid();
}

static job_t *find_job_by_jid(int jid) {
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].used && jobs[i].jid == jid) {
            return &jobs[i];
        }
    }

    return NULL;
}

static job_t *find_job_by_pid(pid_t pid) {
    int i;
    int j;

    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used) {
            continue;
        }

        for (j = 0; j < jobs[i].npids; j++) {
            if (jobs[i].pids[j] == pid) {
                return &jobs[i];
            }
        }
    }

    return NULL;
}

static job_t *find_foreground_job(void) {
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].used && jobs[i].state == JOB_FOREGROUND) {
            return &jobs[i];
        }
    }

    return NULL;
}

static void update_fg_pgid(void) {
    job_t *job = find_foreground_job();
    fg_pgid = (job == NULL) ? 0 : job->pgid;
}

static const char *job_state_name(job_state_t state) {
    switch (state) {
        case JOB_FOREGROUND:
            return "Foreground";
        case JOB_BACKGROUND:
            return "Running";
        case JOB_STOPPED:
            return "Stopped";
        default:
            return "Unknown";
    }
}

static void list_jobs(void) {
    sigset_t prev;
    int i;

    block_sigchld(&prev);

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].used) {
            printf("[%d] %-10s %s\n", jobs[i].jid, job_state_name(jobs[i].state),
                   jobs[i].cmdline);
        }
    }

    restore_sigmask(&prev);
}

static int parse_job_id(const char *arg, int *jid) {
    char *endptr;

    if (arg == NULL || arg[0] != '%') {
        fprintf(stderr, "job id must be in the form %%jobid\n");
        return -1;
    }

    *jid = (int)strtol(arg + 1, &endptr, 10);
    if (*(arg + 1) == '\0' || *endptr != '\0' || *jid <= 0) {
        fprintf(stderr, "invalid job id: %s\n", arg);
        return -1;
    }

    return 0;
}

static int builtin_bg(char **argv) {
    sigset_t prev;
    job_t *job;
    int jid;

    if (parse_job_id(argv[1], &jid) < 0) {
        return 1;
    }

    block_sigchld(&prev);
    job = find_job_by_jid(jid);
    if (job == NULL) {
        restore_sigmask(&prev);
        fprintf(stderr, "%%%d: No such job\n", jid);
        return 1;
    }

    job->state = JOB_BACKGROUND;
    update_fg_pgid();
    restore_sigmask(&prev);

    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("bg");
    } else {
        printf("[%d] %d %s\n", job->jid, job->pgid, job->cmdline);
    }

    return 1;
}

static int builtin_fg(char **argv) {
    sigset_t prev;
    job_t *job;
    pid_t pgid;
    int jid;

    if (parse_job_id(argv[1], &jid) < 0) {
        return 1;
    }

    block_sigchld(&prev);
    job = find_job_by_jid(jid);
    if (job == NULL) {
        restore_sigmask(&prev);
        fprintf(stderr, "%%%d: No such job\n", jid);
        return 1;
    }

    job->state = JOB_FOREGROUND;
    update_fg_pgid();
    pgid = job->pgid;
    restore_sigmask(&prev);

    if (kill(-pgid, SIGCONT) < 0) {
        perror("fg");
        return 1;
    }

    wait_for_foreground(pgid);
    return 1;
}

static int builtin_kill(char **argv) {
    sigset_t prev;
    job_t *job;
    int jid;

    if (parse_job_id(argv[1], &jid) < 0) {
        return 1;
    }

    block_sigchld(&prev);
    job = find_job_by_jid(jid);
    restore_sigmask(&prev);

    if (job == NULL) {
        fprintf(stderr, "%%%d: No such job\n", jid);
        return 1;
    }

    if (kill(-job->pgid, SIGTERM) < 0) {
        perror("kill");
    }

    return 1;
}

static void wait_for_foreground(pid_t pgid) {
    sigset_t empty;

    sigemptyset(&empty);
    while (fg_pgid == pgid) {
        sigsuspend(&empty);
    }
}

static void sigchld_handler(int sig) {
    int olderrno = errno;
    int status;
    pid_t pid;

    (void)sig;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        job_t *job = find_job_by_pid(pid);

        if (job == NULL) {
            continue;
        }

        if (WIFSTOPPED(status)) {
            job->state = JOB_STOPPED;
        } else if (WIFCONTINUED(status)) {
            if (job->state == JOB_STOPPED) {
                job->state = JOB_BACKGROUND;
            }
        } else if (WIFSIGNALED(status) || WIFEXITED(status)) {
            if (job->live_count > 0) {
                job->live_count--;
            }
            if (job->live_count == 0) {
                delete_job(job);
            }
        }
    }

    update_fg_pgid();
    errno = olderrno;
}

static void sigint_handler(int sig) {
    pid_t pgid = fg_pgid;

    (void)sig;

    if (pgid > 0) {
        kill(-pgid, SIGINT);
    }
}

static void sigtstp_handler(int sig) {
    pid_t pgid = fg_pgid;

    (void)sig;

    if (pgid > 0) {
        kill(-pgid, SIGTSTP);
    }
}
