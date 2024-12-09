#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_LINE 1000
#define READ_SIDE 0
#define WRITE_SIDE 1

void add_character_to_string(char* str, char c) {
    // int len = strlen(str);
    // str[len] = c;
    // str[len + 1] = '\0';

    int len = strlen(str);
    if (len < MAX_LINE - 1) { // Prevent buffer overflow
        str[len] = c;
        str[len + 1] = '\0';
    }
}

void split(char* cmd, char* words[], char delimiter) {
    int word_count = 0;
    char* next_char = cmd;
    char current_word[1000] = "";

    while (*next_char != '\0') {
        if (*next_char == delimiter) {
            if (strlen(current_word) > 0) {
                words[word_count] = malloc(strlen(current_word) + 1);  // Allocate memory for the word
                strcpy(words[word_count], current_word);  // Copy the word
                word_count++;
                current_word[0] = '\0';  // Reset current_word to start a new word
            }
        } else {
            add_character_to_string(current_word, *next_char);
        }
        ++next_char;
    }
    if (strlen(current_word) > 0) {
        words[word_count] = malloc(strlen(current_word) + 1);  // Allocate memory for the word
        strcpy(words[word_count], current_word);  // Copy the word
        word_count++;
    }

    words[word_count] = NULL;
}

// true = found in path, false = not found in path
bool find_absolute_path(char* cmd, char* absolute_path) {
    char* directories[1000];

    split(getenv("PATH"), directories, ':');

    // look in array until I find the path + cmd
    for (int ix = 0; directories[ix] != NULL; ix++) {
        char path[1000];
        strcpy(path, directories[ix]);
        add_character_to_string(path, '/');
        strcat(path, cmd);

        if (access(path, X_OK) == 0) {
            strcpy(absolute_path, path);
            return true;
        }
    }
    return false;
}

void execute_command(char* cmd, char* args[]) {
    char absolute_path[1000];
    if (!find_absolute_path(cmd, absolute_path)) {
        fprintf(stderr, "Command '%s' not found\n", cmd);
        exit(1);
    }
    execve(absolute_path, args, NULL);
    fprintf(stderr, "Failed to execute %s\n", absolute_path);
    exit(1);
}

void handle_output_redirection(char* command, char* output_file) {
    int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (output_fd == -1) {
        perror("open");
        exit(1);
    }

    int pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(output_fd);

        char* words[1000];
        split(command, words, ' ');
        execute_command(words[0], words);

        // Free allocated memory for words
        for (int i = 0; words[i] != NULL; i++) {
            free(words[i]);
        }
    }

    close(output_fd);
    wait(NULL);
}

void handle_input_redirection(char* command, char* input_file) {
    // Open the input file in read-only mode
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        perror("open");
        exit(1);
    }

    // Fork a child process
    int pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(input_fd);

        char* words[1000];
        split(command, words, ' ');
        execute_command(words[0], words);

        // Free allocated memory for words
        for (int i = 0; words[i] != NULL; i++) {
            free(words[i]);
        }

        exit(0); // Ensure child process exits after execution
    }

    // Close the file descriptor in the parent process
    close(input_fd);

    // Wait for the child process to finish
    wait(NULL);
}

void handle_piping(char* commands[], int num_commands) {
    int pipe_fds[2 * (num_commands - 1)];
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipe_fds + i * 2) == -1) {
            perror("pipe");
            exit(1);
        }
    }

    for (int i = 0; i < num_commands; i++) {
        int pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            if (i > 0) {
                if (dup2(pipe_fds[(i - 1) * 2], STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }
            }

            if (i < num_commands - 1) {
                if (dup2(pipe_fds[i * 2 + 1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }
            }

            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipe_fds[j]);
            }

            char* words[1000];
            split(commands[i], words, ' ');

            execute_command(words[0], words);

            // Free allocated memory for words
            for (int k = 0; words[k] != NULL; k++) {
                free(words[k]);
            }
        }
    }

    for (int ix = 0; ix < 2 * (num_commands - 1); ix++) {
        close(pipe_fds[ix]);
    }

    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

// Handle cd command
void handle_cd(char* path) {
    if (chdir(path) != 0) {
        perror("cd");
    }
}

// Handle set command to set environment variables
void handle_set(char* var, char* value) {
    if (setenv(var, value, 1) != 0) {
        perror("setenv");
    }
}

// Handle unset command to unset environment variables
void handle_unset(char* var) {
    if (unsetenv(var) != 0) {
        perror("unsetenv");
    }
}

// Expand variables like $FOO
void expand_variables(char* command) {
    char expanded[MAX_LINE];
    int i = 0, j = 0;

    while (command[i] != '\0') {
        if (command[i] == '$') {
            i++;
            char var_name[MAX_LINE] = "";
            int var_len = 0;
            
            // Collect the variable name
            while (command[i] != '\0' && (isalpha(command[i]) || command[i] == '_')) {
                var_name[var_len++] = command[i++];
            }
            var_name[var_len] = '\0';

            // Get the value of the variable
            char* var_value = getenv(var_name);
            if (var_value != NULL) {
                // If variable is found, expand it
                while (*var_value != '\0') {
                    expanded[j++] = *var_value++;
                }
            } else {
                // If the variable is not found, do not add anything
                // This effectively removes the $FOO from the command
            }
        } else {
            expanded[j++] = command[i++];
        }
    }
    expanded[j] = '\0';
    strcpy(command, expanded);  // Copy expanded result back to command
}

int main(int argc, char *argv[]) {
    char line[MAX_LINE];
    while (1) {
        printf("xsh> ");
        if (fgets(line, MAX_LINE, stdin) == NULL) {
            break;
        }
        if (!strcmp(line, "exit\n") || !strcmp(line, "quit\n")) {
            break;
        }

        line[strcspn(line, "\n")] = 0; // Remove newline character

        // Handle setting environment variables
        if (strncmp(line, "set ", 4) == 0) {
            char* var_name = line + 4;
            char* var_value = strchr(var_name, ' ');
            if (var_value != NULL) {
                *var_value = '\0';  // Split the variable name and value
                var_value++;        // Skip the space
                setenv(var_name, var_value, 1);  // Set the environment variable
                printf("%s=%s\n", var_name, var_value);
            }
            continue;
        }

        // Handle unsetting environment variables
        if (strncmp(line, "unset ", 6) == 0) {
            char* var_name = line + 6;
            unsetenv(var_name);  // Unset the environment variable
            continue;
        }

        // Handle background execution with '&'
        int background = 0;
        if (line[strlen(line) - 1] == '&') {
            background = 1;
            line[strlen(line) - 1] = '\0';  // Remove the '&' at the end
        }

        // Expand variables (like $FOO) before executing the command
        expand_variables(line);

        char* commands[1000];

        if (strstr(line, " < ")) {
            char* token = strtok(line, " < ");
            commands[0] = token;
            token = strtok(NULL, " < ");
            char* input_file = token;

            // Remove any extra spaces around the input file name
            while (*input_file == ' ') input_file++;

            handle_input_redirection(commands[0], input_file);
        } else if (strstr(line, " > ")) {
            char* token = strtok(line, " > ");
            commands[0] = token;
            token = strtok(NULL, " > ");
            char* output_file = token;

            // Remove any extra spaces around the output file name
            while (*output_file == ' ') output_file++;

            handle_output_redirection(commands[0], output_file);
        } else {
            split(line, commands, '|');

            int num_commands = 0;
            while (commands[num_commands] != NULL) {
                num_commands++;
            }

            if (num_commands > 1) {
                handle_piping(commands, num_commands);
            } else {
                char* words[1000];
                split(line, words, ' ');
                int child_pid = fork();
                if (child_pid == 0) {
                    execute_command(words[0], words);
                }

                // If it's not a background command, wait for the child to finish
                if (!background) {
                    wait(NULL);
                }
            }
        }

        // // Free allocated memory for commands and words
        // for (int i = 0; commands[i] != NULL; i++) {
        //     free(commands[i]);
        // }

        if (background) {
            printf("Background process started\n");
        }

        printf("\n");
    }

    return 0;
}