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

char *resolve(char *program_name);

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    char *redirect_file = NULL;
    bool is_redirect_in = false;
    bool is_redirect_out = false;
    int saved_stdout;
    int saved_stdin;

    /* prepare args for execv (+ for redirection) */
    size_t args_len = tokens_get_length(tokens);
    char *new_args[args_len];
    unsigned int i;
    for (i = 0; i < args_len && (!is_redirect_in || !is_redirect_out); i++) {
      char *cur_token = tokens_get_token(tokens, i);
      if (strcmp(cur_token, ">") == 0) {
        is_redirect_out = true;
        break;
      } else if (strcmp(cur_token, "<") == 0) {
        is_redirect_in = true;
        break;
      } else {
        new_args[i] = cur_token;
      }
    }
    new_args[i] = NULL;

    // handle redirection
    if (is_redirect_in) { // <
      redirect_file = tokens_get_token(tokens, i + 1);

      int redirect_fd = open(redirect_file, O_RDONLY);
      // save stdout so we can reset the redirection after our program execution
      saved_stdin = dup(STDIN_FILENO); 
      // redirect stdout to file
      dup2(redirect_fd, STDIN_FILENO);
      close(redirect_fd);

    } else if (is_redirect_out) { // >
      redirect_file = tokens_get_token(tokens, i + 1);

      int redirect_fd = open(redirect_file, O_CREAT | O_TRUNC | O_WRONLY);
      // save stdout so we can reset the redirection after our program execution
      saved_stdout = dup(STDOUT_FILENO); 
      // redirect stdout to file
      dup2(redirect_fd, STDOUT_FILENO);
      close(redirect_fd);
    }

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      // fprintf(stdout, "This shell doesn't know how to run programs.\n");

      int status;
      pid_t cpid = fork();
      if (cpid > 0) {
        // running in parent process: wait until child process completes
        // and then continue listening for more commands
        wait(&status);

        // execv done. reset redirection
        if (is_redirect_in) {
          // reset redirection back to STDIN
          dup2(saved_stdin, STDIN_FILENO);
          close(saved_stdin);
        } else if (is_redirect_out) {
          // reset redirection back to STDOUT
          dup2(saved_stdout, STDOUT_FILENO);
          close(saved_stdout);
        }
      } else if (cpid == 0) {
        // runnning in child process: do the work here
        char *path_to_new_program = resolve(tokens_get_token(tokens, 0));\
        if (path_to_new_program == NULL) {
          fprintf(stdout, "This shell doesn't know how to run programs.\n");
          exit(0);
        } else {
          execv(path_to_new_program, new_args);

          /* execv doesn’t return when it works. So, if we got here, it failed! */
          perror("execv failed");
          exit(-1);
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
  }

  return 0;
}


/* Look for a program called PROGRAM_NAME in each directory listed in 
  the PATH environment variable and return first one it finds. Returns
  NULL if it doesn't exists. */
char *resolve(char *program_name) {
  // no need to resolve path if absolute path
  if (access(program_name, F_OK) == 0) {
    return program_name;
  }

  char *path = getenv("PATH");

  // split path by colon
  char *dir = strtok(path, ":");
  while (dir != NULL) {
    char *file_name = malloc(sizeof(char) * 4096);
    if (file_name == NULL) {
      perror("filename malloc failed");
      exit(-1);
    }

    // append program_name to end of each directory in PATH
    // and check if it that file exists
    strcpy(file_name, dir);
    strcat(file_name, "/");
    strcat(file_name, program_name);  
    if (access(file_name, F_OK) == 0)  {
      // file exists
      return file_name;
    }
    // file doesn't exists
    free(file_name);
    dir = strtok(NULL, ":");
  }

  return NULL;
}