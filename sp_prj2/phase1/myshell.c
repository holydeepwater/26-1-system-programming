#include "myshell.h"

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
    char* argv[MAXARGS];
    char buf[MAXLINE];
    pid_t pid;
    strcpy(buf, cmdline);
    parseline(buf, argv);

    if (argv[0] == NULL) return;
    if (!builtin_command(argv)) { //빌트인명령어가 맞으면 함수 안으로 들어가서 실행, 아니면 자식 프로세스 만들어서 실행
        pid = Fork();

        if (pid == 0){ //child 이면
            Execvp(argv[0], argv);
            exit(1);
        }
        if (pid > 0){
            Waitpid(pid, NULL, 0);
        }
    }
    return;
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
    char *token;
    buf[strcspn(buf, "\n")] = '\0';
    token = strtok(buf, " \t");
    while (token != NULL){
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return 0;
}