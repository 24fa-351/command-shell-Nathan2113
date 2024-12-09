#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

void add_character_to_string(char* str, char c);
void split(char* cmd, char* words[], char delimiter);
bool find_absolute_path(char* cmd, char* absolute_path);
void execute_command(char* cmd, char* args[]);

#endif // UTILS_H
