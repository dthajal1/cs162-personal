#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "prints current working directory"},
    {cmd_cd, "cd", "takes one argument, a directory path, and changes the current working directory to that directory"}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Prints current working directory. */
int cmd_pwd(struct tokens* tokens) {
  int PATH_MAX  = 4096;
  char *buf = malloc(sizeof(char) * PATH_MAX);
  if (buf != NULL) {
    getcwd(buf, PATH_MAX);
    printf("%s\n", buf);
    free(buf);
  }
  return 1;
}

/* Takes one argument, a directory path, and changes the current working directory to that directory. */
int cmd_cd(struct tokens* tokens) {
  char *dir_path = tokens_get_token(tokens, 1);
  chdir(dir_path);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

char *resolve(char *program_name, char *path);

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {

    /* Count number of processes */
    char *temp = malloc(sizeof(char) * strlen(line));
    if (temp == NULL) {
      perror("filename malloc failed");
      exit(-1);
    }
    strncpy(temp, line, strlen(line));
    int count;
    for (count=0; temp[count]; temp[count]=='|' ? count++ : *temp++);
    // should free temp somehow??

    int num_pipes = count;
    int fd[num_pipes * 2];
    
    // int fd[2];
    pid_t pid;
    // int pipe_fd_in = 0;
    char *next_process = NULL;

    /* parent creates all needed pipes at the start */
    for(int i = 0; i < num_pipes; i++ ){
      if(pipe(fd + i * 2) < 0 ){
        perror("pipe failed");
        exit(-1);
      }
    }

    bool is_pipe = strstr(line, "|") != NULL;
    int process_count = 0;

    // split different processes separated by |
    char *processes = strtok(line, "|");
    while (processes != NULL) {
      char *ith_process = processes;

      // if (is_pipe) {
      //   if (pipe(fd) == -1) {
      //     perror("pipe failed");
      //     exit(-1);
      //   }
      // }
      
      /* Split a processes args into words. */
      struct tokens* tokens = tokenize(ith_process);

      char *redirect_file_in = NULL;
      char *redirect_file_out = NULL;
      int is_redirect_in = 0;
      int is_redirect_out = 0;
      int saved_stdout;
      int saved_stdin;

      /* prepare args for execv (+ for redirection) */
      size_t args_len = tokens_get_length(tokens);
      char *new_args[args_len];
      unsigned int j = 0;
      for (unsigned int i = 0; i < args_len && (!is_redirect_in || !is_redirect_out); i++) {
        char *cur_token = tokens_get_token(tokens, i);
        if (strcmp(cur_token, ">") == 0) {
          is_redirect_out += 1;
          redirect_file_out = tokens_get_token(tokens, i + 1);
          i++;
          // break;
        } else if (strcmp(cur_token, "<") == 0) {
          is_redirect_in += 1;
          redirect_file_in = tokens_get_token(tokens, i + 1);
          i++;
          // break;
        } else {
          new_args[j] = cur_token;
          j++;
        }
      }
      new_args[j] = NULL;

      // handle redirection
      if (is_redirect_in == 1 && is_redirect_out == 1) {
        int redirect_fd_in = open(redirect_file_in, O_RDONLY);
        // save stdout so we can reset the redirection after our program execution
        saved_stdin = dup(STDIN_FILENO); 
        // redirect stdout to file
        dup2(redirect_fd_in, STDIN_FILENO);
        close(redirect_fd_in);

        
        int redirect_fd_out = open(redirect_file_out, O_CREAT | O_TRUNC | O_RDWR, 0777);
        // save stdout so we can reset the redirection after our program execution
        saved_stdout = dup(STDOUT_FILENO); 
        // redirect stdout to file
        dup2(redirect_fd_out, STDOUT_FILENO);
        close(redirect_fd_out);

      } else if (is_redirect_in) { // <
        int redirect_fd_in = open(redirect_file_in, O_RDONLY);
        // save stdin so we can reset the redirection after our program execution
        saved_stdin = dup(STDIN_FILENO); 
        // redirect stdin to file
        dup2(redirect_fd_in, STDIN_FILENO);
        close(redirect_fd_in);

      } else if (is_redirect_out) { // >
        int redirect_fd_out = open(redirect_file_out, O_CREAT | O_TRUNC | O_RDWR, 0777);
        // save stdout so we can reset the redirection after our program execution
        saved_stdout = dup(STDOUT_FILENO); 
        // redirect stdout to file
        dup2(redirect_fd_out, STDOUT_FILENO);
        close(redirect_fd_out);
      }

      /* Find which built-in function to run. */
      int fundex = lookup(tokens_get_token(tokens, 0));

      if (fundex >= 0) {
        cmd_table[fundex].fun(tokens);
      } else {
        /* REPLACE this to run commands as programs. */
        // fprintf(stdout, "This shell doesn't know how to run programs.\n");

        int status;
        pid = fork();
        if (pid == 0) {
          // runnning in child process: do the work here
          char *path_to_new_program = resolve(tokens_get_token(tokens, 0), getenv("PATH"));
          if (path_to_new_program == NULL) {
            fprintf(stdout, "This shell doesn't know how to run programs.\n");
            exit(0);
          } else {
            if (is_pipe) {
              /* child gets input from the previous command,
              if it's not the first command */
              if (process_count > 1) {
                if (dup2(fd[(process_count - 1) * 2], STDIN_FILENO) < 0) {
                  perror("pipe failed");
                  exit(-1);
                }
              }

              /* child outputs to next command, if it's not
                the last command */
              next_process = strtok(NULL, "|");
              if (next_process != NULL) {
                if (dup2(fd[process_count * 2 + 1], STDOUT_FILENO) < 0) {
                  perror("pipe failed");
                  exit(-1);
                }
              }

              // close all pipe-fds
              for(int i = 0; i < 2 * num_pipes; i++){
                close(fd[i]);
              }
            }
            
            execv(path_to_new_program, new_args);

            /* execv doesn’t return when it works. So, if we got here, it failed! */
            perror("execv failed");
            exit(-1);
          }
        } else if (pid > 0) {
          // running in parent process: wait until child process completes
          // and then continue listening for more commands
          // wait(&status);

          // execv done. reset redirection
          if (is_redirect_in == 1 && is_redirect_out == 1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);

            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
          } else if (is_redirect_in) {
            // reset redirection back to STDIN
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
          } else if (is_redirect_out) {
            // reset redirection back to STDOUT
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
          }

          
          if (is_pipe) {
            /* parent closes all of its copies at the end */
            for(int i = 0; i < 2 * num_pipes; i++) {
              close(fd[i]);
            }

            wait(&status);
          }
          
        } else {
          // foking failed
          perror("Fork failed");
        }
      }

      if (shell_is_interactive)
        /* Please only print shell prompts when standard input is not a tty */
        fprintf(stdout, "%d: ", ++line_num);

      /* Clean up memory */
      tokens_destroy(tokens);

      if (processes == NULL) {
        processes = strtok(NULL, "|");
      } else {
        processes = next_process;
      }
      process_count += 1;
    }
  }
  return 0;
}


/* Look for a program called PROGRAM_NAME in each directory listed in 
  the PATH environment variable and return first one it finds. Returns
  NULL if it doesn't exists. */
char *resolve(char *program_name, char *path) {
  // no need to resolve path if absolute path
  if (access(program_name, F_OK) == 0) {
    return program_name;
  }

  if (strlen(path) == 0) {
    return NULL;
  }

  char *cur_dir = malloc(sizeof(char) * strlen(path));
  if (cur_dir == NULL) {
    perror("filename malloc failed");
    exit(-1);
  }

  // set up to copy path upto first occurence of :
  int len_to_copy = 0;
  unsigned int i;
  for (i = 0; i < strlen(path); i++) {
    if (path[i] == ':') {
      break;
    } else {
      len_to_copy++;
    }
  }

  // append program_name to end of each directory in PATH
  // and check if it that file exists
  strncpy(cur_dir, path, len_to_copy);
  strcat(cur_dir, "/");
  strcat(cur_dir, program_name);  
  if (access(cur_dir, F_OK) == 0)  {
    // file exists
    return cur_dir;
  }
  // file doesn't exists
  free(cur_dir);

  // prepare path for next call
  path += len_to_copy + 1;
  return resolve(program_name, path);
}