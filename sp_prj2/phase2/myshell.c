#include "myshell.h"
#include <ctype.h>

int main(void){
    char cmdline[MAXLINE];
    while (1) {
        printf("CSE4100-SP-P2> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin)) exit(0);
        eval(cmdline);
    }
}
 

void eval(char* cmdline){
    char buf[MAXLINE];
    strcpy(buf, cmdline);
    
    if (strchr(buf, '|') != NULL) {
        char* cmds[MAXCMDS];
        int num_cmds = 0;
        char* token = strtok(buf, "|");
        while (token != NULL && num_cmds < MAXCMDS) {
            cmds[num_cmds++] = token;
            token = strtok(NULL, "|");
        }
        run_pipe(cmds, num_cmds);
        return;
    }
    char *argv[MAXARGS];
    parseline(buf, argv);
    if (argv[0] == NULL) return;
    if (!builtin_command(argv)) {
        pid_t pid = Fork();
        if (pid == 0){
            Execvp(argv[0], argv);
            exit(1);
        }
        Waitpid(pid, NULL, 0);
    }
}

void unix_error(const char* msg){
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

int builtin_command(char** argv){
    if (!strcmp(argv[0], "cd")){ // parent
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        }
        else if (chdir(argv[1]) != 0){
            perror("cd error");
        }
        return 1;
    }
    if (!strcmp(argv[0], "exit")) exit(0); //parent
    return 0; // 빌트인 명령어 아님 
}

pid_t Fork(void) 
{
    pid_t pid;
    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}

void Execvp(const char *file, char *const argv[]) 
{
    if (execvp(file, argv) < 0)
	unix_error("Execvp error");
}

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;
    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
	unix_error("Waitpid error");
    return(retpid);
}

int parseline(char *buf, char **argv){
    int argc = 0;
    char *p = buf;
    buf[strcspn(buf, "\n")] = '\0';

    while (*p) {
        while (*p && isspace((unsigned char) *p)) p++;
        if (!*p) break;
        if(*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[argc++] = p;
            while (*p && *p != quote) p++;
            if (*p == quote) *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && !isspace((unsigned char) *p)) p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[argc] = NULL;
    return argc;
}

int Pipe(int fd[2]) {
    if (pipe(fd) < 0) {
        unix_error("Pipe error");
        return -1;
    }
    return 0;
}

void run_pipe(char **cmds, int num_cmds) {
    if (num_cmds <= 0) return;
    if (num_cmds == 1) {
        char *argv[MAXARGS];
        parseline(cmds[0], argv);
        if (argv[0] == NULL) exit(0);
        if (!builtin_command(argv)) {
            Execvp(argv[0], argv);
        }
        exit(0);
    }

    int fd[2];
    Pipe(fd);

    if(Fork() == 0){ //왼
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        char *argv[MAXARGS];
        parseline(cmds[0], argv);
        if (argv[0] == NULL) exit(0);
        if (!builtin_command(argv)){
            Execvp(argv[0], argv);
        }
        exit(0);
    }
    if(Fork() == 0){ //오
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        run_pipe(cmds+1, num_cmds-1);
        exit(0);
    }
    close(fd[0]);
    close(fd[1]);
    Waitpid(-1, NULL, 0);
    Waitpid(-1, NULL, 0);
}