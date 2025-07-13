#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int main() {
    
    while(1) {
        char line[1024] = {0};
        char *args[64] = {0};
        int i = 0;
        
        printf("lzsh> ");
        fgets(line, sizeof(line), stdin);
        line[strcspn(line, "\n")] = '\0'; //removing the new line at the back

        char *linePtr = strtok(line, " "); //tokenizing everything between space in the input

        while(linePtr != 0) {
            args[i++] = linePtr; //adding into the array the token and every other 
            linePtr = strtok(NULL, " "); //continuing tokenizing after the first one where it stopped
        }    
        args[i] = NULL;


        if (strcmp(args[0], "cd") == 0) {

            if (args[1] == NULL || strcmp(args[1], "~") == 0) {
                chdir(getenv("HOME"));
            }
            else {
                int ret_dir = chdir(args[1]);
                if (ret_dir != 0) {
                    printf("Value of errno: %d\n", errno);
                }
                else {
                    printf("success\n");
                }
                // printf("%s\n", args[0]);
                // printf("%s\n", getcwd(*args, 64));
            }
        }
        else {
            pid_t proc_fork = fork();
            if (proc_fork == 0) {
                execvp(args[0], args);
                perror("exec failed");
                exit(1);
            }
            else if (proc_fork > 0) {
                wait(NULL);
            }
            else {
                perror("fork failed");
            }
        }
        memset(args, 0, sizeof(args));
    }
    return 0;
}
