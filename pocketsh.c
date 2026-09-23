/*
Program purpose: Interactive Command line Shell with basic features like input redirection,
                 file handling,
*/

// references
//  A sample parser file I found two weeks after starting this
//   Remzi H Arpaci-Dusseau and Andrea C Arpaci-Dusseau - Operating Systems: Three Easy Pieces - Chapter 5
//  Michael Kerrisk - The Linux Programming Interface - Chapters 5, 6, 20, 21, 24, 25, 26
//  Brian W. Kernighan and Dennis M. Ritchie -"The C Programming Language" - Chapter 5 for some of the string stuff in the for loops
// the man pages - exec family, strstr, sprintf, strcpy, dup, fork, waitpid, wait, wait

#include "pocketsh.h"
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 512
#define MAX_BUFFER_SIZE 2480

// #ifdef USE_SIGSETJMP
// static sigjmp_buf senv;
// #else
// static jmp_buf env;
// #endif

static volatile sig_atomic_t canJump = 0;
static volatile sig_atomic_t fgMode = 0;
static sigjmp_buf senv;
// void handleRedirects(char **tokens, int *input_fd, int *output_fd, char **cmd_args);

// void handleRedirects(char **tokens, int *input_fd, int *output_fd, char **cmd_args) {}

static void handle(int signal);

static void handle(int signal) {
    fgMode = !fgMode;
    if (canJump) {
        siglongjmp(senv, 1);
    }
}

void executeCommand(char *command, char **args, int input_fd, int output_fd, int *status, int is_bg) {
    int rc = fork();
    char *devNullDir = "/dev/null";

    if (rc < 0)
    {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        // use the explicit redirect if given
        // else fall back to /dev/null
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        } else if (is_bg) {
            int devNull = open(devNullDir, O_RDONLY);
            dup2(devNull, STDIN_FILENO);
            close(devNull);
        }

        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        } else if (is_bg) {
            int devNull = open(devNullDir, O_WRONLY);
            dup2(devNull, STDOUT_FILENO);
            close(devNull);
        }

        struct sigaction sa_dfl = {0};
        sa_dfl.sa_handler = is_bg ? SIG_IGN : SIG_DFL;

        sigaction(SIGINT, &sa_dfl, NULL);

        struct sigaction sa_ign = {0};
        sa_ign.sa_handler = SIG_IGN;
        sigaction(SIGTSTP, &sa_ign, NULL);

        execvp(command, args);
        perror(command);
        exit(1);
    }
    else
    {
        // foreground
        if (is_bg == 0) {
            waitpid(rc, status, 0);
            // if (WIFCONTINUED(x)) {
            if (WIFSIGNALED(*status)) {
                printf("killed by signal %d\n", WTERMSIG(*status));
            }
        }
        // background
        else {
            printf("[bg] started %d\n", rc);
            fflush(stdout);
        }
    }
}

int parseInput(char *buffer, char **tokens)
{
    int i = 0;
    tokens[i] = strtok(buffer, " \n");

    while (tokens[i] != NULL) {
        tokens[++i] = strtok(NULL, " \n");
    }

    return i;
}

//  pid - process identifier process id process id
//  pid_t - process id type process id type process id type
//  pid_t getpid(void) - return the process id of the current process
//  pid_t getppid(void) -  returns the process ID of the parent of the current
//  process fork() - creates a child process wait() - instruct the parent
//  process to wait for a child process to finish whatever it's doing
// makes the code deterministic
// fork, wait, execvp
//  waitpid() - idk yet, maybe it waits on a passed in pid
// exec() - when you want to run a program that is different from the calling
// program kill() signal()
// int main(int argc, char **argv)
// dead children lol

int main()
{
    int lastFgStatus = 0;

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    // don't use this
    // if (sigaction(SIGINT, &sa, NULL) == -1) {
    //   // errExit("sigaction");
    // }

    struct sigaction sa_tstp;
    sa_tstp.sa_handler = handle;
    sigfillset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, NULL);

    if (sigsetjmp(senv, 1) != 0) {
        if (fgMode) {
            printf("\nbackground jobs disabled (& ignored)\n");
        } else {
            printf("\nbackground jobs enabled\n");
        }
        fflush(stdout);
    }
    canJump = 1;

    while (1) {
        char buffer[MAX_BUFFER_SIZE];
        char *tokens[MAX_ARGS];

        int bgStatus;
        pid_t bgPid;
        // bg output exit must come before the prompt
        // print the pid and wait status of each child we reap, without blocking
        while ((bgPid = waitpid(-1, &bgStatus, WNOHANG)) > 0) {
            if (WIFEXITED(bgStatus)) {
                printf("[bg] %d exited with %d\n", bgPid, WEXITSTATUS(bgStatus));
            } else if (WIFSIGNALED(bgStatus)) {

                printf("[bg] %d killed by signal %d\n", bgPid, WTERMSIG(bgStatus));
            }
        }

        // printf("pocketsh ~ ");
        printf("psh> ");
        fflush(stdout);
        fgets(buffer, sizeof(buffer), stdin);

        char pidStr[20];
        // get and copy the actual pid
        //  find the $$ in the buffer,
        //  and replace it with pid from pidstr
        sprintf(pidStr, "%d", getpid());
        int pidLen = strlen(pidStr);

        char expanded[MAX_BUFFER_SIZE];
        char *src = buffer;
        char *dst = expanded;

        while (*src) {
            if (src[0] == '$' &&
                src[1] == '$') {
                    strcpy(dst, pidStr);
                    dst += pidLen;
                    src += 2; // $$
            }
            else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';

        int i = parseInput(expanded, tokens);

        // detect the & in the last token and
        // remove it from the tokens before returning
        int isBg = 0;
        if (i > 0 && strcmp(tokens[i - 1], "&") == 0) {
            if (fgMode) {
                isBg = 0;
            } else {
                isBg = 1;
            }

            tokens[i - 1] = NULL;
            i--;
        }

        // blank lines and comments
        if (tokens[0] == NULL || tokens[0][0] == '#')
        {
            continue;
        }

        if (strcmp(tokens[0], "status") == 0)
        {
            if (WIFEXITED(lastFgStatus)) {
                printf("exit %d\n", WEXITSTATUS(lastFgStatus));
            }
            else if (WIFSIGNALED(lastFgStatus)) {
                printf("killed by signal %d\n", WTERMSIG(lastFgStatus));
            }
            continue;
        }

        if (strcmp(tokens[0], "exit") == 0)
        {
            break;
        }

        if (strcmp(tokens[0], "cd") == 0)
        {
            if (tokens[1] == NULL) {
                chdir(getenv("HOME"));
            }
            else {
                chdir(tokens[1]);
            }
            continue;
        }

        // char *inputFile = NULL;
        // char *outputFile = NULL;
        int inputFd = STDIN_FILENO;
        int outputFd = STDOUT_FILENO;
        int redirectErr = 0;

        for (int j = 1; tokens[j] != NULL;
             j++) {
            // if (strcmp(tokens[j], "<") == 0 || strcmp(tokens[j], ">") == 0) {
            if (strcmp(tokens[j], "<") == 0 && tokens[j + 1] != NULL) {
                inputFd = open(tokens[j + 1], O_RDONLY);
                if (inputFd == -1) {
                    fprintf(stderr, "psh: %s: cannot open for reading\n", tokens[j + 1]);
                    lastFgStatus = 1;
                    redirectErr = 1;
                    break;
                }
                // cut the arguments off
                tokens[j] = NULL;
                // skip filename
                j++;
            }
            // else if (redirectType == '&')
            else if (strcmp(tokens[j], ">") == 0 && tokens[j + 1] != NULL) {
                outputFd = open(tokens[j + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (outputFd == -1) {
                    fprintf(stderr, "psh: %s: cannot open for writing\n", tokens[j + 1]);
                    lastFgStatus = 1;
                    redirectErr = 1;
                    break;
                }
                // cut off the args
                // and skip filename
                tokens[j] = NULL;
                j++;
            }
        }

        if (!redirectErr) {
            executeCommand(tokens[0], tokens, inputFd, outputFd, &lastFgStatus, isBg);
        }
    }

    return 0;
}

// make
