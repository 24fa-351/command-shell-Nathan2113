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
    int len = strlen(str);
    if (len < MAX_LINE - 1) {
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
                words[word_count] = malloc(strlen(current_word) + 1);
                strcpy(words[word_count], current_word);
                word_count++;
                current_word[0] = '\0';
            }
        } else {
            add_character_to_string(current_word, *next_char);
        }
        ++next_char;
    }
    if (strlen(current_word) > 0) {
        words[word_count] = malloc(strlen(current_word) + 1);
        strcpy(words[word_count], current_word);
        word_count++;
    }

    words[word_count] = NULL;
}

bool find_absolute_path(char* cmd, char* absolute_path) {
    char* directories[1000];
    split(getenv("PATH"), directories, ':');

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

void handle_output_redirection(char* line) {
    char* output_file = strstr(line, " > ");
    if (!output_file) return;

    *output_file = '\0';
    output_file += 3;
    while (*output_file == ' ') output_file++;

    char* words[1000];
    split(line, words, ' ');

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
        execute_command(words[0], words);
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
            } else if (output_file != NULL) {
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

            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipe_fds[j]);
            }

            char* words[1000];
            split(commands[i], words, ' ');
            execute_command(words[0], words);
            exit(0);
        }
    }

    for (int ix = 0; ix < 2 * (num_commands - 1); ix++) {
        close(pipe_fds[ix]);
    }

    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

void handle_cd(char* path) {
    if (path == NULL || strcmp(path, "") == 0) {
        char* home_dir = getenv("HOME");
        if (home_dir != NULL) {
            if (chdir(home_dir) != 0) {
                perror("cd");
            }
        } else {
            fprintf(stderr, "cd: HOME not set\n");
        }
    } else {
        if (chdir(path) != 0) {
            perror("cd");
        }
    }
}

void handle_set(char* var, char* value) {
    if (setenv(var, value, 1) != 0) {
        perror("setenv");
    }
}

void handle_unset(char* var) {
    if (unsetenv(var) != 0) {
        perror("unsetenv");
    }
}

void expand_variables(char* command) {
    char expanded[MAX_LINE];
    int ix = 0, j = 0;

    while (command[ix] != '\0') {
        if (command[ix] == '$') {
            ix++;
            char var_name[MAX_LINE] = "";
            int var_len = 0;

            while (command[ix] != '\0' && (isalpha(command[ix]) || command[ix] == '_')) {
                var_name[var_len++] = command[ix++];
            }
            var_name[var_len] = '\0';

            char* var_value = getenv(var_name);
            if (var_value != NULL) {
                while (*var_value != '\0') {
                    expanded[j++] = *var_value++;
                }
            }
        } else {
            expanded[j++] = command[ix++];
        }
    }
    expanded[j] = '\0';
    strcpy(command, expanded);
}

void handle_command(char* line) {
    char* commands[1000];

    if (strncmp(line, "set ", 4) == 0) {
        char* var_name = line + 4;
        char* var_value = strchr(var_name, ' ');
        if (var_value != NULL) {
            *var_value = '\0';
            var_value++;
            handle_set(var_name, var_value);
        }
        return;
    }

    if (strncmp(line, "unset ", 6) == 0) {
        char* var_name = line + 6;
        handle_unset(var_name);
        return;
    }

    if (strncmp(line, "cd ", 3) == 0) {
        char* path = line + 3;
        handle_cd(path);
        return;
    }

    int background = 0;
    if (line[strlen(line) - 1] == '&') {
        background = 1;
        line[strlen(line) - 1] = '\0';
    }

    expand_variables(line);

    if (strstr(line, " < ")) {
        char* token = strtok(line, " < ");
        commands[0] = token;
        token = strtok(NULL, " < ");
        if (token != NULL) {
            handle_input_redirection(commands[0], token);
        }
        return;
    }

    if (strstr(line, " > ")) {
        handle_output_redirection(line);
        return;
    }

    if (strstr(line, " | ")) {
        split(line, commands, '|');
        int num_commands = 0;
        while (commands[num_commands] != NULL) {
            num_commands++;
        }
        handle_piping(commands, num_commands, NULL);
        return;
    }

    split(line, commands, ' ');

    int child_pid = fork();
    if (child_pid == 0) {
        execute_command(commands[0], commands);
    }

    if (!background) {
        wait(NULL);
    }

    if (background) {
        printf("Background process started\n");
    }
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

        line[strcspn(line, "\n")] = 0;

        handle_command(line);
    }

    return 0;
}
