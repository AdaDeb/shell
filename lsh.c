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
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"


/*
 * Function declarations
 */

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);
void simple_command(Command *);
char * search_path(char *);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void)
{
  Command cmd;
  int n;

  while (!done) {

    char *line;
    line = readline("> ");

    if (!line) {
      /* Encountered EOF at top level */
      done = 1;
    }
    else {
      /*
       * Remove leading and trailing whitespace from the line
       * Then, if there is anything left, add it to the history list
       * and execute it.
       */
      stripwhite(line);

      if(*line) {
        add_history(line);
        /* execute it */
        n = parse(line, &cmd);
        //PrintCommand(n, &cmd); // uncomment to print parser info
	simple_command(&cmd);
      }
    }
    
    if(line) {
      free(line);
    }
  }
  return 0;
}

/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void
PrintCommand (int n, Command *cmd)
{
  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void
PrintPgm (Pgm *p)
{
  if (p == NULL) {
    return;
  }
  else {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */

    if (p->next == NULL) printf("ONLY ONE COMMAND");

    PrintPgm(p->next);
    printf("    [");
    while (*pl) {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void
stripwhite (char *string)
{
  register int i = 0;

  while (whitespace( string[i] )) {
    i++;
  }
  
  if (i) {
    strcpy (string, string + i);
  }

  i = strlen( string ) - 1;
  while (i> 0 && whitespace (string[i])) {
    i--;
  }

  string [++i] = '\0';
}


/*
  Runs a simple command
  Maybe should return child exit stat
 */
void simple_command(Command *cmd){
  Pgm *p = cmd->pgm;
  char **pl = p->pgmlist;
  
  int pid = fork();
  int status;

  if (pid == 0) {// child process
    char *prg = search_path(*pl); // free this?
    if (prg == NULL) {
      printf("%s: command not found \n", *pl);
    } else execvp(prg, pl);
  }
  else if (pid > 0){ 
    wait(&status);
  } else 
    printf("Failed to fork!!");
}


char * search_path(char *executable){
  char *envvar = getenv("PATH");
  size_t length  = strlen (envvar) + 1;
  char * pathvar = malloc (length); // allocate memory for array
  strcpy (pathvar, envvar); 
  char *path_tokens = strtok(pathvar, ":");
  
  while(path_tokens != NULL) {

    struct stat info;

    char *fullpath = malloc(strlen(path_tokens)+
			    strlen(executable)+1); // allocate mem for concatenated string
    
    strcpy(fullpath, path_tokens);
    strcat(fullpath, "/");
    strcat(fullpath, executable);

    if (stat(fullpath, &info) == 0){ // fails if file doesn't exist 
      return fullpath;
    }
    path_tokens = strtok(NULL, ":");
  }
  
  free(pathvar);

  return NULL;

}


