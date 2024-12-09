#include "command_handler.h"
#include "io_operations.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>

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
        perror("setenv failed");
    }
}

void handle_unset(char* var) {
    if (unsetenv(var) != 0) {
        perror("unsetenv failed");
    }
}

void expand_variables(char* command) {
    char expanded[MAX_LINE];
    int ix = 0, jx = 0;

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
                    expanded[jx++] = *var_value++;
                }
            }
        } else {
            expanded[jx++] = command[ix++];
        }
    }
    expanded[jx] = '\0';
    strcpy(command, expanded);
}

void handle_command(char* line) {
    char* commands[1000];

    // Handle 'set' command
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

    // Handle 'unset' command
    if (strncmp(line, "unset ", 6) == 0) {
        char* var_name = line + 6;
        handle_unset(var_name);
        return;
    }

    // Handle 'cd' command
    if (strncmp(line, "cd ", 3) == 0) {
        char* path = line + 3;
        handle_cd(path);
        return;
    }

    // Handle background process command
    int background = 0;
    if (line[strlen(line) - 1] == '&') {
        background = 1;
        line[strlen(line) - 1] = '\0'; // Remove the '&' character
    }

    expand_variables(line);

    // Handle input redirection
    if (strstr(line, " < ")) {
        char* token = strtok(line, " < ");
        commands[0] = token;
        token = strtok(NULL, " < ");
        if (token != NULL) {
            handle_input_redirection(commands[0], token);
        }
        return;
    }

    char* output_file = strstr(line, " > ");
    char* pipe_start = strstr(line, " | ");
    
    // Handle output redirection
    if (output_file) {
        *output_file = '\0';
        output_file += 3;    
        while (*output_file == ' ') output_file++; 

        split(line, commands, ' '); 

        // If piping exists, handle it separately
        if (pipe_start) {
            split(line, commands, '|');
            int num_commands = 0;
            while (commands[num_commands] != NULL) {
                num_commands++;
            }
            // Handle piping with output redirection
            handle_piping(commands, num_commands, output_file);
            return;
        } else {
            handle_output_redirection(commands, output_file);
            return;
        }
    }

    // // Handle piping
    // if (pipe_start) {
    //     split(line, commands, '|');
    //     int num_commands = 0;
    //     while (commands[num_commands] != NULL) {
    //         num_commands++;
    //     }
    //     // Handle piping (no redirection)
    //     handle_piping(commands, num_commands, NULL);
    //     return;
    // }

    // If no redirection or piping, split and execute the command normally
    split(line, commands, ' ');

    // Execute the command normally
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

