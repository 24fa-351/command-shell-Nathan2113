#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#define MAX_LINE 1000

void handle_cd(char* path);
void handle_set(char* var, char* value);
void handle_unset(char* var);
void expand_variables(char* command);
void handle_command(char* line);

#endif // COMMAND_HANDLER_H