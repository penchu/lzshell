#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h> 
#include <fcntl.h>
#include <signal.h>

#define MAX_WORD_LENGTH 128
#define MAX_JOBS 10

typedef int (*ptr_func)(int argc, char *argv[]);

static volatile bool working = true;

int test = 0;
int test2 = 1;

enum State {
    Running = 1,
    Done = 0,
    Stopped = -1
};

struct Command {
    const char *name;
    const char *descr;
    bool pipeable;
    ptr_func handler;
};

struct Job_table {
    int job_num;
    pid_t pid;
    char *command;
    enum State status;
    // int status;
};

char *read_input();
int parse_input(char **args, char *line, int *n, int *redirection_type, bool *redirection, bool *piping, bool *background, char **piped_cmd, char *redirection_file);
int cmd_dispatch(int (**builtin_handler)(int argc, char *argv[]), char **args, int n, bool piping);
int cmd_cd(int argc, char **args);
int cmd_echo(int argc, char **args);
int cmd_help(); 
int cmd_exit();
int cmd_path();
int run_external(int n, int (*builtin_handler)(int argc, char *argv[]), char **args, int redirection_type, bool redirection, bool piping, bool *background, char **piped_cmd, char *redirection_file);
int redirection_draft(char **args, int redirection_type, char *redirection_file);
int cleanup(char **args, char *line, char *redirection_file, char **piped_cmd, int *n, int *redirection_type, bool *piping);
void sig_handler(int signum, siginfo_t *info, void *context);

struct Command help_list[] = {
    {"cd", "Change directory.", false, cmd_cd},
    {"ext", "Exit the shell.", false, cmd_exit},
    {"wd", "Print working directory.", true, cmd_path},
    {"ech", "Write arguments to the standard output.", true, cmd_echo},
    {"help", "Display information about builtin commands.", true, cmd_help},    
};

struct Job_table jobs_list[MAX_JOBS];

int main() {
    // bool working = true;   
    char *line = NULL;
    char *args[MAX_WORD_LENGTH] = {0}; //array for each token of the input
    char redirection_file[MAX_WORD_LENGTH] = {0};
    char *piped_cmd[MAX_WORD_LENGTH] = {0};
    int n = 0; //number of tokens
    bool redirection = false; //for if > is encountered 
    bool piping = false;
    bool background = false;
    int redirection_type;  
    int (*builtin_handler)(int argc, char *argv[]) = NULL; 

    struct sigaction sig;
    // sig.sa_handler = sig_handler;
    sig.sa_sigaction = sig_handler;
    sig.sa_flags = SA_SIGINFO;
    sigemptyset(&sig.sa_mask);

    sigaction(SIGCHLD, &sig, NULL);

    while (working) { 
        // for (int i = 0; jobs_list[i].job_num != '\0'; i++) {
        //     if (jobs_list[i].pid == test2) {
        //         printf("[%d] Done.\n", jobs_list[i].job_num);
        //         break;
        //     }
        // }

        // if (test == test2) printf("[1] Done.\n");
        printf("lzsh> ");
        line = read_input();
        if (!line) {
            if (errno == 4) {
                errno = 0;
            }
            else {
                working = false;
            }
        }
        else {
            parse_input(args, line, &n, &redirection_type, &redirection, &piping, &background, piped_cmd, redirection_file);
            if (!cmd_dispatch(&builtin_handler, args, n, piping)) run_external(n, builtin_handler, args, redirection_type, redirection, piping, &background, piped_cmd, redirection_file);
        }
        // for (int i = 0; args[i] != NULL; i++) { //printf for checking the arrays
        //     // printf("args[%d] = %s\n", i, args[i]);
        //     printf("piped_cmd[%d] = %s\n", i, piped_cmd[i]);
        // }
        cleanup(args, line, redirection_file, piped_cmd, &n, &redirection_type, &piping);        
    }
    return 0;
}

char *read_input() {
    char *buff = calloc(1024, 1);
    if (buff == NULL) {
        perror("malloc");
        exit(1);
    }
    
    if (fgets(buff, 1024, stdin) == NULL && errno != 4) {
        cmd_exit();
        return NULL;
    } 
    buff[strcspn(buff, "\n")] = '\0'; //removing the new line at the back and terminating it 
    return buff;
}

int parse_input (char **args, char *line, int *n, int *redirection_type, bool *redirection, bool *piping, bool *background, char **piped_cmd, char *redirection_file) {
    int m = 0; //for characters in each token
    bool space; //for space and tab detection for echo cmd

    int n_pipe = 0;
    int m_pipe = 0;
    bool space_pipe;

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
                if ((line[i] == ' ' || line[i] == '\t') && (!space_pipe)) { 
                    space_pipe = true;
                    piped_cmd[++(n_pipe)] = calloc(MAX_WORD_LENGTH, 1);
                    m_pipe = 0;
                    i++;
                    continue;
                }
                else if ((line[i] == ' ' || line[i] == '\t') && space_pipe) continue;
                piped_cmd[n_pipe][m_pipe++] = line[i++];
                space_pipe = false;
            }
            // piped_cmd[n_pipe+1] = NULL; //additional code, to be checked
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
    
    if (strcmp(args[*n], "&") == 0) {//checking for a & for running a background process, set the flag and replace with a NULL
        *background = true;
        args[*n] = NULL;
    }
    else args[*n+1] = NULL;

    n_pipe = 0; 
    m_pipe = 0; 
    return 0;
}

int cmd_dispatch(int (**builtin_handler)(int argc, char *argv[]), char **args, int n, bool piping) {
    int help_len = sizeof(help_list) / sizeof(help_list[0]);
    for (int i = 0; args[i] != NULL; i++) {
        // if (strcmp(args[i], "ext") == 0) {
        //     working = false;
        //     return 1;
        // }            
        for (int j = 0; j < help_len; j++) {
            if (strcmp(args[i], help_list[j].name) == 0) {
                if (help_list[j].pipeable && piping) {
                    *builtin_handler = help_list[j].handler;  
                    return 0;
                }
                help_list[j].handler(n, args);
                return 1;       
            }
        }
    }
    return 0;
}

int cmd_cd(int argc, char **args) {
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

int cmd_echo(int argc, char **args) {
    int echo_count = 1; //for tracking where the command stops and the text starts
    char flag[6] = {0}; //flag with the echo cmd
    if (strcmp(args[1], "-n") == 0) {
        echo_count++;
        flag[0] = 'n';
    }
    else if (strcmp(args[1], "-e") == 0) {
        echo_count++; 
        flag[0] = 'e';
    } 
    for (int i = echo_count; args[i] != NULL; i++) {
        for (int j = 0; args[i][j] != '\0'; j++) {
            if (flag[0] == 'e') {
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
        if (args[i+1] != NULL) {
            printf(" ");
        }
    }
    if (!(flag[0] == 'n')) { //checking if there is a flag -n no new line
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

int run_external(int n, int (*builtin_handler)(int argc, char *argv[]), char **args, int redirection_type, bool redirection, bool piping, bool *background, char **piped_cmd, char *redirection_file) {
    int fd_p[2];
    if (piping) pipe(fd_p); //creating a pipe with the array of 2 int needed dor it and checking for an error
    if (fd_p < 0) {  
        perror("pipe");
        exit(1);
    }
    
    pid_t proc_fork = fork();//for external commands, creating a child process  
    if (proc_fork == 0) { //return value 0 is for the child process  
        if (piping) { 
            dup2(fd_p[1], 1);
            close(fd_p[0]);
            if (builtin_handler) {  
                builtin_handler(n, args); //trying to implement calling the function if the command is builtin, maybe should be done somewhere else
                close(fd_p[1]);
                _exit(0);
            }
            else {
                execvp(args[0], args);
                perror("exec failed1");
                exit(1);
            }
        }
        else { 
            if (redirection) redirection_draft(args, redirection_type, redirection_file); //it's changing the output from the terminal to the file 
            execvp(args[0], args);
            perror("exec failed2");
            exit(1);
        }
    }
    else if (proc_fork > 0) { //parent process return value is 1

        // if (*background) {
        //     jobs_list[0].job_num = 1;
        //     jobs_list[0].pid = proc_fork;
        //     jobs_list[0].status = Running;
        //     printf("[%d] %d\n", jobs_list[0].job_num, jobs_list[0].pid);
        //     *background = false;
        // }

        int count = 0;
        if (*background) {            
            printf("[1] %d\n", proc_fork);
            test = proc_fork;
            *background = false;
            // jobs_list[count].pid = proc_fork;
            // jobs_list[count].status = 1;
        }        
        
        if (piping) {
            pid_t pipe_fork = fork(); //creating another child process for the other end of the pipe
            if (pipe_fork == 0) {
                close(fd_p[1]);
                dup2(fd_p[0], 0);
                close(fd_p[0]);
                execvp(piped_cmd[0], piped_cmd);
                perror("exec failed3");
                exit(1);
            }
            else if (pipe_fork > 0) {
                close(fd_p[1]);
                close(fd_p[0]);
                waitpid(proc_fork, NULL, 0);
                waitpid(pipe_fork, NULL, 0);
            }
        }
        else {
            wait(NULL);
        }
    }
    else {
        perror("fork failed");
    }
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
    else if (redirection_type == 0) {
        int fd = open(redirection_file, O_RDONLY);
        if (fd < 0) {
            perror("open2"); 
            exit(1); 
        }
        dup2(fd, 0);
        close(fd);
    }    
    // memset(redirection_file, 0, sizeof(redirection_file));
    // redirection_file[0] = '\0'; //should work almost as the memset, but zeroing out the first will stop the access
    return 0;
}

int cleanup(char **args, char *line, char *redirection_file, char **piped_cmd, int *n, int *redirection_type, bool *piping) {
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
        args[i] = NULL; //optional, suggested by chatgpt
    }
    memset(args, 0, sizeof(args));
    free(line);
    line = NULL; //optional, suggested by chatgpt
    
    for (int i = 0; piped_cmd[i] != NULL; i++) {
        free(piped_cmd[i]);
        piped_cmd[i] = NULL;
    }
    memset(piped_cmd, 0, sizeof(piped_cmd));

    *n = 0;
    // memset(redirection_file, 0, sizeof(redirection_file)); 
    redirection_file[0] = '\0';
    *redirection_type = 2;
    // piped_cmd[0] = '\0';
    *piping = false;
    return 0;
}

int cmd_path() {
    char *ptr = getcwd(NULL, 0);
    if (ptr) {
        printf("%s\n", ptr);
        free(ptr);
    }
    else {
        perror("getcwd");
        exit(1);
    }
    return 0;
}

int cmd_exit() {
    working = false;
    return 0;
}

void sig_handler(int signum, siginfo_t *info, void *context) {
    int status;
    // waitpid(-1, &status, WNOHANG); 

    // test = info->si_pid;
    waitpid(info->si_pid, &status, WNOHANG);
    test2 = info->si_pid;

    // printf("[1] Done.\n");
}