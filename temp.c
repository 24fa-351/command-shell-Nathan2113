
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <stdbool.h>
// #include <fcntl.h>
// #include <sys/wait.h>

// #define MAX_LINE 1000
// #define COMMAND_SEPARATOR "|"
// #define READ_SIDE 0
// #define WRITE_SIDE 1

// void add_character_to_string(char* str, char command) {
//     int len = strlen(str);
//     str[len] = command;
//     str[len + 1] = '\0';
// }

// // splits string by spaces; adds a NULL into the array after the last word
// void split(char* cmd, char* words[], char delimiter) {
//     // int word_count = 0;
//     // char* next_char = cmd;
//     // char current_word[1000] = "";
//     // strcpy(current_word, "");

//     // while (*next_char != '\0') {
//     //     if (*next_char == delimiter) {
//     //         words[word_count++] = strdup(current_word);
//     //         strcpy(current_word, "");
//     //     } else {
//     //         add_character_to_string(current_word, *next_char);
//     //     }
//     //     ++next_char;
//     // }
//     // words[word_count++] = strdup(current_word);

//     // words[word_count] = NULL;

//     int word_count = 0;
//     char* next_char = cmd;
//     char current_word[1000] = "";

//     while (*next_char != '\0') {
//         if (*next_char == delimiter) {
//             if (strlen(current_word) > 0) {
//                 words[word_count++] = strdup(current_word);
//                 current_word[0] = '\0';  // Reset for next word
//             }
//         } else {
//             add_character_to_string(current_word, *next_char);
//         }
//         ++next_char;
//     }
//     if (strlen(current_word) > 0) {  // Add the last word
//         words[word_count++] = strdup(current_word);
//     }
//     words[word_count] = NULL;  // NULL-terminate the array
// }

// // true = found in path, false = not found in path
// bool find_absolute_path(char* cmd, char* absolute_path) {
//     char* directories[1000];

//     split(getenv("PATH"), directories, ':');

//     // look in array until I find the path + cmd
//     for (int ix = 0; directories[ix] != NULL; ix++) {
//         char path[1000];
//         strcpy(path, directories[ix]);
//         add_character_to_string(path, '/');
//         strcat(path, cmd);

//         if (access(path, X_OK) == 0) {
//             strcpy(absolute_path, path);
//             return true;
//         }
//     }
//     return false;
// }

// void execute_command(char* cmd, char* args[]) {
//     char absolute_path[1000];
//     if (!find_absolute_path(cmd, absolute_path)) {
//         fprintf(stderr, "Command '%s' not found\n", cmd);
//         exit(1);
//     }

//     // char* newargs[1000];
//     // int ix = 0;

//     // while(args[ix + 1] != NULL) {
//     //     newargs[ix] = args[ix + 1];
//     //     ix++;
//     // }

//     // printf("Arguements: %s\n", args[0]);
//     execve(absolute_path, args, NULL);
//     fprintf(stderr, "Failed to execute %s\n", absolute_path);
//     exit(1);
// }

// void handle_redirection(char* output_file, char* cmd, char* args[]) {
//     int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
//     if (output_fd == -1) {
//         fprintf(stderr, "Failed to open %s\n", output_file);
//         exit(1);
//     }

//     int child_pid = fork();
//     if (child_pid == 0) {
//         dup2(output_fd, STDOUT_FILENO); /* STDOUT_FILENO = 1 */
//         close(output_fd);
//         execute_command(cmd, args);
//     }

//     wait(NULL); // If we have '&', we need to actually wait
//     close(output_fd);
// }

// void handle_piping(char* first_cmd[], char* second_cmd[]) {
//     int pipe_from_read_to_write[2];
//     if (pipe(pipe_from_read_to_write) == -1) {
//         perror("pipe failed");
//         exit(1);
//     }

//     int command1_pid = fork();
//     if (command1_pid == -1) {
//         perror("fork failed");
//         exit(1);
//     }

//     if (command1_pid == 0) {
//         // first command process
//         close(pipe_from_read_to_write[READ_SIDE]);
//         dup2(pipe_from_read_to_write[WRITE_SIDE], STDOUT_FILENO);
//         close(pipe_from_read_to_write[WRITE_SIDE]);
//         execute_command(first_cmd[0], first_cmd);
//     }

//     int command2_pid = fork();
//     if (command2_pid == -1) {
//         perror("fork");
//         exit(1);
//     }

//     if (command2_pid == 0) {
//         // second command process
//         close(pipe_from_read_to_write[WRITE_SIDE]);
//         dup2(pipe_from_read_to_write[READ_SIDE], STDIN_FILENO);
//         close(pipe_from_read_to_write[READ_SIDE]);
//         execute_command(second_cmd[0], second_cmd);
//     }

//     close(pipe_from_read_to_write[READ_SIDE]);
//     close(pipe_from_read_to_write[WRITE_SIDE]);

//     waitpid(command1_pid, NULL, 0);
//     waitpid(command2_pid, NULL, 0);
// }

// int main(int argc, char *argv[]) {
//     char line[MAX_LINE];
//     while (1) {
//         printf("xsh> ");
//         if (fgets(line, MAX_LINE, stdin) == NULL) {
//             break;
//         }
//         if (!strcmp(line, "exit\n") || !strcmp(line, "quit\n")) {
//             break;
//         }

//         line[strcspn(line, "\n")] = 0; // Remove newline character

//         char* commands[2];
//         char* output_file = NULL;
//         char* words[1000];

//         if (strstr(line, " > ")) {
//             char* token = strtok(line, " > ");
//             commands[0] = token;
//             token = strtok(NULL, " > ");
//             output_file = token;
//             split(commands[0], words, ' ');
//             handle_redirection(output_file, words[0], words);
//         } else if (strstr(line, " | ")) {
//             char* token = strtok(line, " | ");
//             commands[0] = token;
//             token = strtok(NULL, " | ");
//             commands[1] = token;

//             char* first_cmd[1000];
//             char* second_cmd[1000];
//             split(commands[0], first_cmd, ' ');
//             split(commands[1], second_cmd, ' ');

//             handle_piping(first_cmd, second_cmd);
//         } else {
//             split(line, words, ' ');
//             int child_pid = fork();
//             if (child_pid == 0) {
//                 execute_command(words[0], words);
//             }
//             wait(NULL);
//         }
//     }

//     return 0;
// }