#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h> 
#include <fcntl.h>

struct Command {
    const char *name;
    const char *descr;
};

struct Command help_list[] = {
    {"cd", "Change directory."},
    {"ext", "Exit the shell."},
    {"ech", "Write arguments to the standard output."},
    {"help", "Display information about builtin commands."},    
};

#define MAX_WORD_LENGTH 128

char *read_input();
int parse_input(char **args, char *line, int *n, int *redirection_type, bool *redirection, bool *piping, char **piped_cmd, char *redirection_file);
int cmd_cd(char **args);
int cmd_echo(char **args, int n);
int cmd_help();
int run_external(char **args, int n, int redirection_type, bool redirection, bool piping, char **piped_cmd, char *redirection_file);
int redirection_draft(char **args, int redirection_type, char *redirection_file);

int main() {
    bool working = true;   
    // char line[1024] = {0}; //user input
    char *line = NULL;
    char *args[MAX_WORD_LENGTH] = {0}; //array for each token of the input
    char redirection_file[MAX_WORD_LENGTH] = {0};
    char *piped_cmd[MAX_WORD_LENGTH] = {0};
    int n = 0; //number of tokens
    bool redirection = false; //for if > is encountered 
    bool piping = false;
    int redirection_type;      

    while (working) {        
        printf("lzsh> ");
        line = read_input();
        // if (redirection) printf("success1\n");
        parse_input(args, line, &n, &redirection_type, &redirection, &piping, piped_cmd, redirection_file);
        // if (redirection) printf("success2.5\n");

        // for (int i = 0; args[i] != NULL; i++) { //pirntf for checking the arrays
        //     printf("args[%d] = %s\n", i, args[i]);
        //     printf("piped_cmd[%d] = %s\n", i, piped_cmd[i]);
        // }

        if (strcmp(args[0], "cd") == 0) cmd_cd(args);
        else if (strcmp(args[0], "ext") == 0) {
            working = false;
            break;
        }
        else if (strcmp(args[0], "ech") == 0) cmd_echo(args, n);
        else if (strcmp(args[0], "help") == 0) cmd_help();
        else run_external(args, n, redirection_type, redirection, piping, piped_cmd, redirection_file);
        for (int i = 0; i < n; i++) {
            free(args[i]);
            args[i] = NULL; //optional, suggested by chatgpt
        }
        memset(args, 0, sizeof(args));
        free(line);
        line = NULL; //optional, suggested by chatgpt
        n = 0;
        // memset(redirection_file, 0, sizeof(redirection_file)); 
    }
    return 0;
}

char *read_input() {
    char *buff = calloc(1024, 1);
    if (buff == NULL) {
        perror("malloc");
        exit(1);
    }
    
    fgets(buff, 1024, stdin); //when using dynamic memory it sizeof() will just get the memory of the pointer and not the whole thing
    buff[strcspn(buff, "\n")] = '\0'; //removing the new line at the back and terminating it 
    // printf("%s\n", buff);  
    return buff;
}

int parse_input (char **args, char *line, int *n, int *redirection_type, bool *redirection, bool *piping, char **piped_cmd, char *redirection_file) {
    int m = 0; //for characters in each token
    bool space; //for space and tab detection for echo cmd
    args[0] = calloc(MAX_WORD_LENGTH, 1);
    piped_cmd[0] = calloc(MAX_WORD_LENGTH, 1);
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '"') {
            i++;
            while (line[i] != '"') {
                args[*n][m++] = line[i++];
            }
            m = 0;
            continue;   
        }
        if (line[i] == '>' || line[i] == '<') {
            *redirection_type = (line[i] == '<') ? 0 : 1;
            *redirection = true;
            i++;
            i++;
            int j = 0;
            while (line[i] != '\0') {                
                redirection_file[j++] = line[i++]; 
            }
            redirection_file[j] = '\0';
            break;
        }
        if (line[i] == '|') {
            *piping = true;
            i++;
            i++;
            int j = 0;
            while (line[i] != '\0') {                
                piped_cmd[0][j++] = line[i++]; 
            }
            // piped_cmd[j] = '\0';
            break;
        }
        if ((line[i] == ' ' || line[i] == '\t')  && (!space)) {
            space = true;
            if (line[i+1] != '>' && line[i+1] != '<' && line[i+1] != '|') args[++(*n)] = calloc(MAX_WORD_LENGTH, 1); //not allocating memory if there is a redirection, changed malloc with calloc which allocates zeroed memory and no garbage issues
            m = 0;   
            continue;           
        }
        else if ((line[i] == ' ' || line[i] == '\t')  && space) continue;
        args[*n][m++] = line[i];
        space = false;    
    }
    return 0;
}

int cmd_cd(char **args) {
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

int cmd_echo(char **args, int n) {
    int echo_count = 1;
    char flag[6] = {0}; //flag with the echo cmd
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
                if (args[i][j] == '\\' && args[i][j+1] == '\\') j++;
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

int cmd_help() {
    int help_len = sizeof(help_list) / sizeof(help_list[0]);
    for (int i = 0; i < help_len; i++) {
        printf("%s - %s\n", help_list[i].name, help_list[i].descr);
    }
    return 0;
}

int run_external(char **args, int n, int redirection_type, bool redirection, bool piping, char **piped_cmd, char *redirection_file) {
    int fd_p[2];
    if (piping && pipe(fd_p) < 0) { //creating a pipe with the array of 2 int needed dor it and checking for an error 
        perror("pipe");
        exit(1);
    }

    pid_t proc_fork = fork(); //for external commands, creating a child process     
    if (proc_fork == 0) { //return value 0 is for the child process
        
        // printf("%s\n", args[0]); // the green out lines below are for checking the args array with 
        // for (int i = 0; args[i] != NULL; i++) { //checking args before running the code, the redirection works strangely
        //     printf("args[%d] = %s\n", i, args[i]);
        // }
        // printf("args[%d] = %p (NULL terminator)\n", n, args[n]);     
        // if (redirection) printf("success3\n");
        
        if (piping) { 
            dup2(fd_p[1], 1);
            close(fd_p[1]);
            close(fd_p[0]);
            execvp(args[0], args);
            perror("exec failed");
            exit(1);
        }    
        if (redirection) redirection_draft(args, redirection_type, redirection_file); //it's changing the output from the terminal to the file 
        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    }
    else if (proc_fork > 0) { //parent process return value is 1
        if (piping) {
            pid_t pipe_fork = fork(); //creating another child process for the other end of the pipe
            if (pipe_fork == 0) {
                dup2(fd_p[0], 0);
                close(fd_p[0]);
                close(fd_p[1]);
                execvp(piped_cmd[0], piped_cmd);
            }
            else {
                close(fd_p[0]);
                close(fd_p[1]);
                wait(NULL);
            }
        }
        wait(NULL);
    }
    else {
        perror("fork failed");
    }
    piped_cmd[0] = '\0';
    return 0;
}

int redirection_draft(char **args, int redirection_type, char *redirection_file) {
    // for (int i = 0; redirection_file[i] != '\0'; i++) {
    //     printf("%c", redirection_file[i]);
    // }
    // printf("\n");   
    // printf("[%c][%c][%c]\n", redirection_file[0], redirection_file[1], redirection_file[2]);   
    
    if (redirection_type == 1) {
        int fd = open(redirection_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open1"); 
            exit(1); 
        }
        dup2(fd, 1);    
        close(fd);    
    }
    else {
        int fd = open(redirection_file, O_RDONLY);
        if (fd < 0) {
            perror("open2"); 
            exit(1); 
        }
        dup2(fd, 0);
        close(fd);
    }    
    // memset(redirection_file, 0, sizeof(redirection_file));
    redirection_file[0] = '\0'; //should work almost as the memste, but zeroing out the first will stop the access
    return 0;
}

