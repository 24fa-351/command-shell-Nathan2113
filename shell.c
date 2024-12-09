#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command_handler.h"
#include "io_operations.h"
#include "utils.h"

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
