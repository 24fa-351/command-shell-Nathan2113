#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

void add_character_to_string(char* str, char c) {
    int len = strlen(str);
    str[len] = c;
    str[len + 1] = '\0';
}

// splits string by spaces; adds a NULL into the array after the last word
void split(char* cmd, char* words[], char delimiter) {
    int word_count = 0;
    char* next_char = cmd;
    char current_word[1000] = "";
    strcpy(current_word, "");

    while (*next_char != '\0') {
        if (*next_char == delimiter) {
            words[word_count++] = strdup(current_word);
            strcpy(current_word, "");
        } else {
            add_character_to_string(current_word, *next_char);
        }
        ++next_char;
    }
    words[word_count++] = strdup(current_word);

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


// ./this <command> <arg1> <arg2> ...
// 1) break argv[2] into words
// 2) find the absolute path of the command

int main(int argc, char* argv[]) {
    char absolute_path[1000];
    char* words[1000];

    split(argv[1], words, ' ');

    if (words[0] == NULL) {
        fprintf(stderr, "No command given\n");
        return 1;
    }

    if ( ! find_absolute_path(words[0], absolute_path)) {
        fprintf(stderr, "Command '%s' not found\n", words[0]);
        return 1;
    }

    for (int ix = 0; words[ix] != NULL; ix++) {
        printf("words[%d] = %s\n", ix, words[ix]);
    }
    printf("absolute path = '%s'\n", absolute_path);
    
    execve(absolute_path, words, NULL);
    printf("Failed to execute %s\n", absolute_path);
    return 1;
}