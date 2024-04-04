#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80             /* 명령어의 최대 길이 */

static void cmdexec(char *cmd, int input_fd, int output_fd)
{
    char *argv[MAX_LINE/2+1];
    int argc = 0;
    char *p, *q;
    bool redirect_input = false;
    char *input_file = NULL;
    bool redirect_output = false;
    char *output_file = NULL;
    bool pipe_cmd = false;
    int pipe_fds[2];

    p = cmd; p += strspn(p, " \t");
    do {
        q = strpbrk(p, " \t\'\"");
        if (q == NULL || *q == ' ' || *q == '\t') {
            q = strsep(&p, " \t");
            if (*q) argv[argc++] = q;
        }
        else if (*q == '\'') {
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
        }
        else {
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
        }        
    } while (p);
    argv[argc] = NULL;
    
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "<") == 0) {
            redirect_input = true;
            input_file = argv[i + 1];
            argv[i] = NULL;
        }
        else if (strcmp(argv[i], ">") == 0) {
            redirect_output = true;
            output_file = argv[i + 1];
            argv[i] = NULL;
        }
        else if (strcmp(argv[i], "|") == 0) {
            pipe_cmd = true;
            argv[i] = NULL;
        }
    }

    if (redirect_input) {
        int fd = open(input_file, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (redirect_output) {
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (pipe_cmd) {
        int pipe_result = pipe(pipe_fds);
        if (pipe_result == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            close(pipe_fds[0]);
            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[1]);
            execvp(argv[0], argv);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else {
            close(pipe_fds[1]);
            dup2(pipe_fds[0], STDIN_FILENO);
            close(pipe_fds[0]);

            char *next_cmd[MAX_LINE/2+1];
            next_cmd[0] = "grep";
            next_cmd[1] = "-i";
            next_cmd[2] = "system";
            next_cmd[3] = NULL;
            execvp(next_cmd[0], next_cmd);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    else {
        execvp(argv[0], argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    char cmd[MAX_LINE+1];
    int len;
    pid_t pid;
    bool background;
    
    while (true) {
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);
        
        printf("tsh> "); fflush(stdout);
        
        len = read(STDIN_FILENO, cmd, MAX_LINE);
        if (len == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        cmd[--len] = '\0';
        if (len == 0)
            continue;
        
        if (!strcasecmp(cmd, "exit"))
            break;
        
        char *p = strchr(cmd, '&');
        if (p != NULL) {
            background = true;
            *p = '\0';
        }
        else
            background = false;
        
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            cmdexec(cmd, STDIN_FILENO, STDOUT_FILENO);
            exit(EXIT_SUCCESS);
        }
        else if (!background)
            waitpid(pid, NULL, 0);
    }
    return 0;
}

