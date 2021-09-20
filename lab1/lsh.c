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

int main(void)
{
  Command cmd;
  int parse_result;

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
  *  For easy system calls without arguments
  */
  //system(*cmd->pgm->pgmlist);

  /* 
  *  For system calls with and without arguments
  */
  //execComm(cmd);

  /* 
  For system calls in the background
    ! Doesn't work
  */
  //exComm(cmd);

  /* 
  * For pipes
  */
  pipeExec(cmd);


  //printf("\n");
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

void sigtest(int sig) {
  if(sig == SIGINT) return;

}

void exComm(Command *cmd)
{
  int status;
  pid_t pid;

  pid = fork();

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
    if(cmd->background){return;}
    // No waiting for child 
    else
    {
      signal(SIGINT, sigtest);
      waitpid(pid, &status, 0);
      return;
    }
  }
}

void pipeExec(Command *cmd)
{
  int piperd[2]; 
  pid_t pid, pid2;

  if (pipe(piperd) < 0)
  {
    printf("\nPipe could not be initialized");
    return;
  }
  pid = fork();
  if (pid < 0)
  {
    printf("\nFork 1 failed");
    return;
  }
  else if (pid == 0)
  {
    // First child executing
    close(piperd[0]);
    dup2(piperd[1], STDOUT_FILENO);
    close(piperd[1]);

    if(execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist) < 0)
    {
      printf("\nExecution failed on child 1");
      exit(0);
    }
  }
  else {
    // Parent executing
    pid2 = fork();
    if (pid2 < 0)
    {
      printf("\nFork 2 failed");
      return;
    }

    // Second child executing
    else if (pid2 == 0) 
    {
      close(piperd[1]);
      dup2(piperd[0], STDIN_FILENO);
      close(piperd[0]);

      if(execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist) < 0)
      {
        printf("\nExecution failed on child 2");
        exit(0);
      }

    }
    else {
      printf("\nFörsta wait\n");
      waitpid(pid, NULL, 0);
      printf("\nAndra wait\n");
      int a = waitpid(pid2, NULL, 0);
      printf("\n%d", a);
    }
  }
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
