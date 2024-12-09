// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <stdbool.h>
// #include "redirect.h"
// #include "find_abs_path.h"

// #define MAX_LINE 1000
// int main(int argc, char *argv[]) {
//     char line[MAX_LINE];
//     while (1) {
//         printf("xsh> ");
//         fgets(line, MAX_LINE, stdin);
//         if (!strcmp(line, "exit\n") || !strcmp(line, "quit\n")) {
//             break;
//         }
//         printf("You entered: %s", line);
//         // and now go split, open pipes, open files, etc AND THEN fork
//     }

//     return 0;
// };

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_LINE 1000
#define COMMAND_SEPARATOR "|"
#define READ_SIDE 0
#define WRITE_SIDE 1

void add_character_to_string(char* str, char c) {
    int len = strlen(str);
    str[len] = c;
    str[len + 1] = '\0';
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

void handle_redirection(char* command, char* output_file) {
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
                while (*var_value != '\0') {
                    expanded[j++] = *var_value++;
                }
            } else {
                // If variable is not found, just append nothing
                expanded[j++] = '$';
                for (int k = 0; k < var_len; k++) {
                    expanded[j++] = var_name[k];
                }
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

        char* commands[1000];

        // Check for cd command
        char* words[1000];
        split(line, words, ' ');

        if (strcmp(words[0], "set") == 0) {
            if (words[1] == NULL || words[2] == NULL) {
                fprintf(stderr, "set: expected variable and value\n");
            } else {
                handle_set(words[1], words[2]);
            }
        } else if (strcmp(words[0], "unset") == 0) {
            if (words[1] == NULL) {
                fprintf(stderr, "unset: expected variable\n");
            } else {
                handle_unset(words[1]);
            }
        } 
        else if (strcmp(words[0], "cd") == 0) {
            if (words[1] == NULL) {
                fprintf(stderr, "cd: expected argument\n");
            } else {
                handle_cd(words[1]);
            }
        }
        // Check for output redirection '>'
        else if (strstr(line, " > ")) {
            char* token = strtok(line, " > ");
            commands[0] = token;
            token = strtok(NULL, " > ");
            char* output_file = token;

            // Remove any extra spaces around the output file name
            while (*output_file == ' ') output_file++;

            handle_redirection(commands[0], output_file);
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
                wait(NULL);
            }
        }

        // Free allocated memory only after the command is executed and all child processes are done
        for (int i = 0; commands[i] != NULL; i++) {
            free(commands[i]);  // Free the memory for each command part
        }
    }

    return 0;
}