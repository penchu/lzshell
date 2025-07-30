#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h> 

#define MAX_WORD_LENGTH 128

int parse_input (char **args, char *line, int *n, int m, bool quotes);
int cmd_cd (char **args);
int cmd_echo (char **args, char *flag, int n);

int main() {
    bool working = true;
    
    while (working) {
        char line[1024] = {0};
        char *args[MAX_WORD_LENGTH] = {0};
        int n = 0;
        int m = 0;
        char flag[6] = {0};
        bool quotes;
        
        printf("lzsh> ");
        fgets(line, sizeof(line), stdin);
        line[strcspn(line, "\n")] = '\0'; //removing the new line at the back
        
        args[0] = malloc(MAX_WORD_LENGTH);
        parse_input(args, line, &n, m, quotes);
        // for (int i = 0; line[i] != '\0'; i++) {
        //     if (line[i] == '"') {
        //         quotes = true;
        //     }
        //     if (!quotes) {
        //         if (line[i] == ' ') {
        //             n++;  
        //             args[n] = malloc(MAX_WORD_LENGTH);
        //             m = 0;   
        //             continue;           
        //         }
        //         args[n][m++] = line[i];
        //     }
        //     else {
        //         while (line[i] != '"') {
        //             args[n][m++] = line[i++];
        //         }
        //         quotes = false;
        //         m = 0;
        //         continue;
        //     }
        // }

        if (strcmp(args[0], "cd") == 0) {
            // if (args[1] == NULL || strcmp(args[1], "~") == 0) {
            //     chdir(getenv("HOME"));
            // }
            // else {
            //     int ret_dir = chdir(args[1]);
            //     if (ret_dir != 0) {
            //         printf("Error: %s\n", strerror(errno));
            //     }
            // }
            cmd_cd(args);
        }
        else if (strcmp(args[0], "ext") == 0) {
            working = false;
            break;
        }
        else if (strcmp(args[0], "ech") == 0) {
            cmd_echo(args, flag, n);
            // int echo_count = 1;
            // if (strcmp(args[1], "-n") == 0) {
            //     echo_count++;
            //     flag[6] = 'n';
            // }
            // else if (strcmp(args[1], "-e") == 0) {
            //     echo_count++; 
            //     flag[6] = 'e';
            // }            
            // for (int i = echo_count; i <= n; i++) {
            //     for (int j = 0; args[i][j] != '\0'; j++) {
            //         if (flag[6] == 'e') {
            //             if (args[i][j] == '\\' && args[i][j+1] == 'n') {
            //                 j++;
            //                 j++;
            //                 printf("\n");
            //             }
            //             if (args[i][j] == '\\' && args[i][j+1] == 't') {
            //                 j++;
            //                 j++;
            //                 printf("\t");
            //             }
            //             if (args[i][j] == '\\' && args[i][j+1] == '\\') {
            //             j++;
            //         }
            //         }
            //         printf("%c", args[i][j]);
            //     }
            //     if (i != n) {
            //         printf(" ");
            //     }
            // }
            // if (!(flag[6] == 'n')) { //checking if there is a flag -n to not include a new line
            //     printf("\n");
            // }
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

int parse_input (char **args, char *line, int *n, int m, bool quotes) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '"') {
            quotes = true;
        }
        if (!quotes) {
            if (line[i] == ' ') {
                (*n)++;  
                args[*n] = malloc(MAX_WORD_LENGTH);
                m = 0;   
                continue;           
            }
            args[*n][m++] = line[i];
        }
        else {
            while (line[i] != '"') {
                args[*n][m++] = line[i++];
            }
            quotes = false;
            m = 0;
            continue;
        }
    }
    return 0;
}

int cmd_cd (char **args) {
    if (args[1] == NULL || strcmp(args[1], "~") == 0) {
        chdir(getenv("HOME"));
    }
    else {
        int ret_dir = chdir(args[1]);
        if (ret_dir != 0) {
            printf("Error: %s\n", strerror(errno));
        }
    }    
    return 0;
}

int cmd_echo (char **args, char *flag, int n) {
    int echo_count = 1;
    if (strcmp(args[1], "-n") == 0) {
        echo_count++;
        flag[6] = 'n';
    }
    else if (strcmp(args[1], "-e") == 0) {
        echo_count++; 
        flag[6] = 'e';
    }    
    for (int i = echo_count; i <= n; i++) {
        for (int j = 0; args[i][j] != '\0'; j++) {
            if (flag[6] == 'e') {
                if (args[i][j] == '\\' && args[i][j+1] == 'n') {
                    j++;
                    j++;
                    printf("\n");
                }
                if (args[i][j] == '\\' && args[i][j+1] == 't') {
                    j++;
                    j++;
                    printf("\t");
                }
                if (args[i][j] == '\\' && args[i][j+1] == '\\') {
                j++;
            }
            }
            printf("%c", args[i][j]);
        }
        if (i != n) {
            printf(" ");
        }
    }
    if (!(flag[6] == 'n')) { //checking if there is a flag -n to not include a new line
        printf("\n");
    }
    return 0;
}
