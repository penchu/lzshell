#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h> 

int main() {
    bool working = true;
    
    while(working) {
        char line[1024] = {0};
        char *args[64] = {0};
        int i = 0;
        char flag[6] = {0};
        
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
                    printf("Error: %s\n", strerror(errno));
                }
            }
        }
        else if (strcmp(args[0], "exit") == 0) {
            working = false;
            break;
        }
        else if (strcmp(args[0], "ech") == 0) {
            int echo_count = 1;
            if (strcmp(args[1], "-n") == 0) {
                echo_count++;
                flag[6] = 'n';
            }
            // if (strcmp(args[1], "-e") == 0) {
            //     echo_count++; 
            //     flag_e = true;
            // }
            // while (args[echo_count] != NULL) {     
            //     printf("%s ", args[echo_count++]);
            // }
            for (int j = echo_count; j < i; j++) {
                if (j == (i-1)) {
                    printf("%s", args[j]);
                }
                else printf("%s ", args[j]);
            }
            if (!(flag[6] == 'n')) {
                printf("\n");
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
