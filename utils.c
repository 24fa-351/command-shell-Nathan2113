#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "command_handler.h"

void add_character_to_string(char* str, char c) {
    int len = strlen(str);
    if (len < MAX_LINE - 1) {
        str[len] = c;
        str[len + 1] = '\0';
    }
}

void split(char* command, char* words[], char delimiter) {
    int word_count = 0;
    char* next_char = command;
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


bool find_absolute_path(char* command, char* absolute_path) {
    char* directories[1000];
    split(getenv("PATH"), directories, ':');

    for (int ix = 0; directories[ix] != NULL; ix++) {
        char path[1000];
        strcpy(path, directories[ix]);
        add_character_to_string(path, '/');
        strcat(path, command);

        if (access(path, X_OK) == 0) {
            strcpy(absolute_path, path);
            return true;
        }
    }
    return false;
}

void execute_command(char* command, char* arguments[]) {
    char absolute_path[1000];
    if (!find_absolute_path(command, absolute_path)) {
        fprintf(stderr, "Command '%s' not found\n", command);
        exit(1);
    }
    execve(absolute_path, arguments, NULL);

    fprintf(stderr, "Failed to execute %s\n", absolute_path);
    exit(1);
}