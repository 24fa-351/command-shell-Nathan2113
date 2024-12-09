#ifndef IO_OPERATIONS_H
#define IO_OPERATIONS_H

#define READ_SIDE 0
#define WRITE_SIDE 1

void handle_output_redirection(char* line[], char* output_file);
void handle_input_redirection(char* command, char* input_file);
void handle_piping(char* commands[], int num_commands, char* output_file);

#endif // IO_OPERATIONS_H
