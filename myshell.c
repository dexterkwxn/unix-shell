/**
 * CS2106 AY22/23 Semester 1 - Lab 2
 *
 * This file contains function definitions. Your implementation should go in
 * this file.
 */

#include "myshell.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct PCBTable pcbtable_array[MAX_PROCESSES];
// pid
// status - 4: stopped, 3: terminating, 2: running, 1: exited
// exitCode - -1 not exit, else exit code status
int next = 0;

void my_init(void) {

  // Initialize what you need here
  for (size_t i = 0; i < MAX_PROCESSES; ++i) {
    struct PCBTable entry = {.pid = -1, .status = 0, .exitCode = -2};
    pcbtable_array[i] = entry;
  }
}

void collect_zombies() {
  for (size_t i = 0; i < MAX_PROCESSES; ++i) {
    if (pcbtable_array[i].status == 2 || pcbtable_array[i].status == 3) {
      int wstatus = 0;
      pid_t child_pid = waitpid(pcbtable_array[i].pid, &wstatus, WNOHANG);
      if (child_pid == pcbtable_array[i].pid) {
        pcbtable_array[i].status = 1;
        pcbtable_array[i].exitCode = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus)
                                     : WIFSIGNALED(wstatus) ? WTERMSIG(wstatus)
                                                            : -1;
      }
    }
  }
}

// handles possible redirection and calls execute_command()
void run_command(size_t num_tokens, char **tokens) {
  if (tokens[0] == NULL)
    return;
  if (access(tokens[0], X_OK) == 0 && access(tokens[0], R_OK) == 0) {

    size_t in_idx = 0;
    size_t out_idx = 0;
    size_t err_idx = 0;
    for (size_t i = 0; i < num_tokens - 1; ++i) {
      if (strcmp(tokens[i], "<") == 0) {
        in_idx = i;
      } else if (strcmp(tokens[i], ">") == 0) {
        out_idx = i;
      } else if (strcmp(tokens[i], "2>") == 0) {
        err_idx = i;
      }
    }

    bool background = strcmp(tokens[num_tokens - 2], "&") == 0;

    int wstatus = 0;
    pid_t child_pid = fork();

    if (child_pid == 0) {
      // format args
      size_t arr_len = !in_idx && !out_idx && !err_idx && background
                           ? num_tokens - 1
                       : in_idx  ? in_idx + 2
                       : out_idx ? out_idx + 1
                       : err_idx ? err_idx + 1
                                 : num_tokens;

      char *argv[arr_len];
      for (size_t i = 0; i < arr_len; ++i) {
        argv[i] = tokens[i];
      }
      if (in_idx) {
        if (access(tokens[in_idx + 1], R_OK) == 0) {
          argv[arr_len - 2] = tokens[arr_len - 1];
        } else {
          fprintf(stderr, "%s does not exist\n", tokens[in_idx + 1]);
          exit(1);
        }
      }
      argv[arr_len - 1] = NULL;

      if (out_idx) {
        FILE *out_file = fopen(tokens[out_idx + 1], "w+");
        dup2(fileno(out_file), STDOUT_FILENO);
        fclose(out_file);
      }
      if (err_idx) {
        FILE *err_file = fopen(tokens[err_idx + 1], "w+");
        dup2(fileno(err_file), STDERR_FILENO);
        fclose(err_file);
      }

      execv(tokens[0], argv);
    } else {
      // parent
      struct PCBTable entry = {.pid = child_pid, .status = 2, .exitCode = -1};
      pcbtable_array[next] = entry;

      if (!background) {
        waitpid(child_pid, &wstatus, 0);
        pcbtable_array[next].exitCode = WEXITSTATUS(wstatus);
        pcbtable_array[next].status = 1;
      } else {
        printf("Child [%d] in background\n", child_pid);
      }
      ++next;
    }
  } else {
    fprintf(stderr, "%s not found\n", tokens[0]);
  }
}

void my_process_command(size_t num_tokens, char **tokens) {

  // Your code here, refer to the lab document for a description of the
  // arguments

  collect_zombies();
  // process command
  if (strcmp(tokens[0], "info") == 0) {
    if (num_tokens != 3) {
      fprintf(stderr, "Wrong command\n");
      return;
    }
    if (strcmp(tokens[1], "0") == 0) {
      for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbtable_array[i].pid == -1) {
          continue;
        }
        if (pcbtable_array[i].status == 1) {
          // exited
          printf("[%d] Exited %d\n", pcbtable_array[i].pid,
                 pcbtable_array[i].exitCode);
        } else if (pcbtable_array[i].status == 2) {
          // running
          printf("[%d] Running\n", pcbtable_array[i].pid);
        } else if (pcbtable_array[i].status == 3) {
          // terminating
          printf("[%d] Terminating\n", pcbtable_array[i].pid);
        } else {
          // Stopped
          printf("[%d] Stopped\n", pcbtable_array[i].pid);
        }
      }
    } else if (strcmp(tokens[1], "1") == 0) {
      // # of exited processes
      int count = 0;
      for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbtable_array[i].status == 1) {
          ++count;
        }
      }
      printf("Total exited process: %d\n", count);
    } else if (strcmp(tokens[1], "2") == 0) {
      // # of running processes
      int count = 0;
      for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbtable_array[i].status == 2) {
          ++count;
        }
      }
      printf("Total running process: %d\n", count);
    } else if (strcmp(tokens[1], "3") == 0) {
      // # of terminating processes
      int count = 0;
      for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbtable_array[i].status == 3) {
          ++count;
        }
      }
      printf("Total terminating process: %d\n", count);
    } else {
      fprintf(stderr, "Wrong command\n");
    }
  } else if (strcmp(tokens[0], "wait") == 0) {
    if (num_tokens == 3) {
      pid_t pid = atoi(tokens[1]);
      for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbtable_array[i].pid == pid && pcbtable_array[i].status == 2) {
          int wstatus = 0;
          waitpid(pid, &wstatus, 0);
          pcbtable_array[i].status = 1;
          pcbtable_array[i].exitCode = WEXITSTATUS(wstatus);
          break;
        }
      }
    }
  } else if (strcmp(tokens[0], "terminate") == 0) {
    if (num_tokens == 3) {
      pid_t pid = atoi(tokens[1]);
      for (size_t i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbtable_array[i].pid == pid && pcbtable_array[i].status == 2) {
          pcbtable_array[i].status = 3;
          kill(pcbtable_array[i].pid, SIGTERM);
          break;
        }
      }
    }

  } else {
    size_t num_tokens_param = 0;
    char **tokens_param = tokens;
    size_t i = 0;
    size_t total_i = 0;
    while (total_i++ < num_tokens - 1) {
      ++num_tokens_param;
      if (strcmp(tokens_param[i], ";") == 0) {
        // run command
        tokens_param[i] = NULL;
        run_command(num_tokens_param, tokens_param);
        tokens_param = &tokens_param[num_tokens_param];
        i = 0;
        num_tokens_param = 0;
      } else {
        ++i;
      }
    }
    run_command(num_tokens_param + 1,
                tokens_param); // +1 for the NULL we skipped
  }
}

void my_quit(void) {

  // Clean up function, called after "quit" is entered as a user command
  for (size_t i = 0; i < MAX_PROCESSES; ++i) {
    if (pcbtable_array[i].status == 2) {
      pcbtable_array[i].status = 3;
      kill(pcbtable_array[i].pid, SIGTERM);
      printf("Killing [%d]\n", pcbtable_array[i].pid);
    }
  }

  printf("\nGoodbye\n");
}
