/* 
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file 
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive: 
      $> tar cvf lab1.tgz lab1/
 *
 * All the best 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

void RunCommand(int, Command *);
void DebugPrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
// My own
void execComm(Command *);
void exComm(Command *);
void pipeExec(Command *);
void sigtest(int);
void redirectExec(Command *);
void handle_sigint_and_sigchld(int);
pid_t child_pid = -1;


int main(void)
{
  Command cmd;
  int parse_result;
  // added to make sure ctrl+c does not work in the beginning. If this is the track we want to go, we need to set the handler to SIG_IGN through  the sa_handler function instead:
  
  /* struct sigaction sa;
  sa.sa_handler = SIG_IGN;   // more details at flylib.com/books/en/4.443.1.96/1/
  */
  
  // sigaction(SIGINT, SIG_IGN, NULL);

  //added to handle ctrl c and children from the start
  struct sigaction sa;
  sa.sa_handler = &handle_sigint_and_sigchld;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  while (TRUE)
  {
    char *line;
    line = readline("> ");

    /* If EOF encountered, exit shell */
    if (!line)
    {
      break;
    }
    /* Remove leading and trailing whitespace from the line */
    stripwhite(line);
    /* If stripped line not blank */
    if (*line)
    {
      add_history(line);
      parse_result = parse(line, &cmd);
      RunCommand(parse_result, &cmd);
    }

    /* Clear memory */
    free(line);
  }
  return 0;
}

/* Execute the given command(s).

 * Note: The function currently only prints the command(s).
 * 
 * TODO: 
 * 1. Implement this function so that it executes the given command(s).
 * 2. Remove the debug printing before the final submission.
 */
void RunCommand(int parse_result, Command *cmd)
{
  /* 
  * For easy system calls without arguments
  */
  //system(*cmd->pgm->pgmlist);

  /* 
  * For system calls with and without arguments
  */
  //execComm(cmd);

  /* 
  * For system calls in the background
  */
  exComm(cmd);

  /* 
  * For pipes
  */
  //pipeExec(cmd);

  /* 
  * For redirect
  */
  redirectExec(cmd);
  //DebugPrintCommand(parse_result, cmd);
}

void execComm(Command *cmd)
{
  pid_t pid = fork();

  if (pid == -1)
  {
    printf("\nFork failed");
    return;
  }
  else if (pid == 0)
  {
    if (execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist) < 0)
    {
      printf("\nCould not execute the command");
    }
    exit(0);
  }
  else
  {
    wait(NULL);
    return;
  }
}

void handle_sigint_and_sigchld(int sig) {
  if(sig == SIGINT){
    if(child_pid != -1){
      kill(child_pid, SIGTERM);
      child_pid = -1;

    }
  }
  if(sig == SIGCHLD){
    // see if any child processes have terminated. If so, the parent will remove them from the process table
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
  }
}

void exComm(Command *cmd)
{
  int status;
  pid_t pid;
  
  if(strcmp("exit", *cmd->pgm->pgmlist) == 0){
    exit(1);
  }
  if(strcmp("cd", *cmd->pgm->pgmlist) == 0){
    // Took inspiration from print_pgm function. 
    char **pl = cmd->pgm->pgmlist;
    char* path = *(pl+1);
    if(chdir(path) != 0){
      printf("Error in path: %s", path);
    }
    return;
  }
  pid = fork();
  if (pid == -1)
  {
    printf("\nFork failed");
    return;
  }
  else if (pid == 0)
  {
    // used to change the gid of the children, to make sure they do not receive the SIGINT that is sent to the parent by pressing CTRL-C.
    // this way, the parent must accept the SIGINT and then send an appropriate signal only to the child that is supposed to be affected by the SIGINT.
    setsid();
    if (execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist) < 0)
    {
      printf("\nCould not execute the command");
    }
    exit(0);
  }
  else
  {
    if (cmd->background)
    {
      return;
    }
    // No waiting for child
    else
    {
      child_pid = pid;
      waitpid(pid, &status, 0);
      child_pid = -1;
      return;
    }
  }
}










void pipeExec(Command *cmd)
{
  int i, cmds = 0, numberOfPipes = 0;
  pid_t pid;
  int pipeFD[2 * numberOfPipes];
  // Head Node to count number of pipes
  Pgm *current = cmd->pgm;

  // Counting number of pipes
  while (current->next != NULL)
  {
    numberOfPipes++;
    current = current->next;
  }

  // Reassign current to the first node
  current = cmd->pgm;

  // ! RÄTT
  for (i = 0; i < numberOfPipes; i++)
  {
    if (pipe(pipeFD + i * 2) < 0)
    {
      printf("\n Failed creating pipes");
      return;
    }
  }

  do
  {
    pid = fork();

    if (pid < 0)
    {
      printf("\n Failed to fork");
    }
    // Child process
    if (pid == 0)
    {

      /* 
      * If we need to redirect the stdout to a pipe read end.
      * Will not be done on the last command since we want to send stdout to terminal.
      */
      if (cmds != 0)
      {
        if (dup2(pipeFD[cmds * 2 - 1], STDOUT_FILENO) < 0)
        {
          perror("\n Error duplicating input");
        }
      }

      /* 
      * If we need to redirect the stdin to a pipe write end.
      * Will not be done to the first command entered in the terminal. 
      */
      if (cmds != numberOfPipes)
      {
        if (dup2(pipeFD[cmds * 2], STDIN_FILENO) < 0)
        {
          printf("\n Error duplicating output");
        }
      }

      // Close all file descriptors
      for (i = 0; i < 2 * numberOfPipes; i++)
      {
        close(pipeFD[i]);
      }

      if (execvp(*current->pgmlist, current->pgmlist) < 0)
      {
        printf("\nError executing");
        exit(0);
      }
    }

    // ! Have to be here to move the loop forward
    current = current->next;
    cmds++;
  } while (cmds < numberOfPipes + 1);

  for (i = 0; i < 2 * numberOfPipes; i++)
  {
    close(pipeFD[i]);
  }

  for (i = 0; i < numberOfPipes + 1; i++)
  {
    waitpid(pid, NULL, 0);
  }
}

void redirectExec(Command *cmd)
{
  int rFile, oFile;
  pid_t pid, pid2;

  pid = fork();

  if(pid < 0)
  {
    perror("pid");
  }

  if (pid == 0)
  {
    if (cmd->rstdin != NULL)
    {

      if (cmd->rstdout != NULL)
      {

        if ((oFile = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
        {
          perror("open create");
          exit(0);
        }
        if(dup2(oFile, STDOUT_FILENO) < 0)
        {
          perror("dup2");
        }
        close(oFile);
      }

      if ((rFile = open(cmd->rstdin, O_RDONLY)) < 0)
      {
        perror("open");
        exit(0);
      }

      if (dup2(rFile, STDIN_FILENO) < 0)
      {
        perror("dup2");
      }
      close(rFile);
      
      pid2 = fork();
      if (pid2 < 0)
      {
        perror("fork");
      }

      if (pid2 == 0)
      {
        execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist);
      }
      else
      {
        waitpid(pid, NULL, 0);
      }
    }
  }
  waitpid(pid, NULL, 0);
  
}

/* 
 * Print a Command structure as returned by parse on stdout. 
 * 
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
void DebugPrintCommand(int parse_result, Command *cmd)
{
  if (parse_result != 1)
  {
    printf("Parse ERROR\n");
    return;
  }
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd->rstdin ? cmd->rstdin : "<none>");
  printf("stdout:     %s\n", cmd->rstdout ? cmd->rstdout : "<none>");
  printf("background: %s\n", cmd->background ? "true" : "false");
  printf("Pgms:\n");
  PrintPgm(cmd->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 * 
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
void PrintPgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    PrintPgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Strip whitespace from the start and end of a string. 
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  register int i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    strcpy(string, string + i);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}