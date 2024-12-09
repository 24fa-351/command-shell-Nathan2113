#include "io_operations.h"
#include "command_handler.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

void handle_output_redirection(char* command[], char* output_file) {
    int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (output_fd == -1) {
        perror("open");
        exit(1);
    }

    int pid = fork();
    if (pid == 0) {
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(output_fd);
        
        execute_command(command[0], command);
    }

    close(output_fd);
    wait(NULL);
}


void handle_input_redirection(char* command, char* input_file) {
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        perror("open");
        exit(1);
    }

    int pid = fork();
    if (pid == 0) {
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(input_fd);
        char* words[1000];
        split(command, words, ' ');
        execute_command(words[0], words);
    }

    close(input_fd);
    wait(NULL);
}

void handle_piping(char* commands[], int num_commands, char* output_file) {
    int pipe_fds[2 * (num_commands - 1)];

    // Set up the pipes
    for (int ix = 0; ix < num_commands - 1; ix++) {
        if (pipe(pipe_fds + ix * 2) == -1) {
            perror("pipe");
            exit(1);
        }
    }

    for (int ix = 0; ix < num_commands; ix++) {
        int pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Handle input redirection (for commands after the first)
            if (ix > 0) {
                if (dup2(pipe_fds[(ix - 1) * 2], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }
            }

            // Handle output redirection (for commands before the last)
            if (ix < num_commands - 1) {
                if (dup2(pipe_fds[ix * 2 + 1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }
            } else if (output_file != NULL) {
                // Final command in the pipe chain: handle output redirection
                int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (output_fd == -1) {
                    perror("open");
                    exit(1);
                }
                if (dup2(output_fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }
                close(output_fd);
            }

            // Close all pipes in the child process
            for (int jx = 0; jx < 2 * (num_commands - 1); jx++) {
                close(pipe_fds[jx]);
            }

            char* words[1000];
            split(commands[ix], words, ' ');
            execute_command(words[0], words);
            exit(0);
        }
    }

    for (int ix = 0; ix < 2 * (num_commands - 1); ix++) {
        close(pipe_fds[ix]);
    }

    for (int ix = 0; ix < num_commands; ix++) {
        wait(NULL);
    }
}
