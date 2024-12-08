#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <outputFile> <command> [<arg1>, <arg2> ...]", argv[0]);
        return 1;
    }

    int output_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (output_fd == -1) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        return 1;
    }

    // add one NULL, -2 cause we're skipping the first two arguments
    char** newargv = (char**)malloc(sizeof(char*) * (1 + argc - 2));

    // we "copy it down" by two
    for (int ix = 2; ix < argc; ix++) {
        printf("Copying %s to newargv[%d]\n", argv[ix], ix - 2);
        newargv[ix - 2] = (char*)argv[ix];
    }
    newargv[argc - 2] = NULL;

    int child_pid = fork();
    if (child_pid == 0) {
        dup2(output_fd, STDOUT_FILENO); /* STDOUT_FILENO = 1 */
        close(output_fd);

        execve(newargv[0], newargv, NULL);
    }

    wait(NULL); // If we have '&', we need to actually wait

    printf("%s pid is %d. forked %d. "
        "Parent exiting\n", argv[0], getpid(), child_pid);

    return 0;
}