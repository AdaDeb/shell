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

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <signal.h>
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
void run_command(Command *);
char * search_path(char *);
void change_dir(char **);
void signal_handler(int);
int is_background(char **);
void handle_pipe(Pgm *, int *);


/* When non-zero, this global means the user is done using this program. */
int done = 0;
int firstRun = 1;
int pCounter = 0;
int pipeNumbers;
int pip;
char *stdinstr;
char *stdoutstr;



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
	run_command(&cmd);
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
  Should have comments inside this method!!
  * author: Adam Debbiche
 */
void run_command(Command *cmd){
  Pgm *p = cmd->pgm;
  char **pl = p->pgmlist;
  stdinstr = cmd->rstdin;
  stdoutstr = cmd->rstdout;
  sigset_t mask, omask;

  signal(SIGINT, signal_handler);
  if (cmd -> bakground == 1) 
    signal(SIGCHLD, signal_handler);
  
  
 
  // Here we should first check if the command has any pipes 
  // if yes then handle them otherwise do what's below

  if (p->next != NULL) {
    pipeNumbers = countPipesNeeded(p);
    pCounter = 0;
    int pipes[pipeNumbers*2];
    pip = 0;
    firstRun = 1;
    handle_pipe(p, pipes);
    pCounter = 0;	   
  } else {

    if (strcmp(*pl,"exit") == 0)
      exit(0); // needs to be tested more
 

    if (strcmp(*pl,"cd") == 0) {
      change_dir(pl);
      return;
    }
    
    int status;  
    char *prg = search_path(*pl); // free this?
    if (prg == NULL) {
	printf("%s: command not found \n", *pl);
	return;
    }
    
    int pid = fork();
    if (pid == 0) {// child process
      if(cmd->bakground)
        signal(SIGINT, SIG_IGN);
      else
	signal(SIGINT, SIG_DFL);

      if (cmd -> rstdin != NULL) {
	FILE *fp;
	fp = freopen (cmd ->rstdin, "r", stdin);
       }
      
      if (cmd->rstdout != NULL){
        mode_t mode =  S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
	int fd = creat(cmd->rstdout,mode);
	dup2(fd, 1);
        close(fd);
      }
      execvp(prg, pl);	
    }
    else if (pid > 0){ 
      if (cmd -> bakground == 0) 
	wait(&status);
    } 
  }  
} 

/*
 * Search for the path of an executable in the
 * PATH variable and returns it
 * author: Adam Debbiche
 */
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


/*
 * Implements the cd command of our shell
 * author: Adam Debbiche
 */
void change_dir(char **pl){ // maybe find better name then pl?
  *pl++;
  chdir(*pl);
}


/*
 * Handles the interruption signal of SIGINT
 * when Ctrl-C is pressed
 * author: Adam Debbiche
 */
void signal_handler(int signal){
  int status;
  if (signal == SIGINT) {return;} 
  else { // got signal from a background child process 
    waitpid(NULL, &status,WNOHANG);
  }
}

/*
 * Counts the number of pipes
 * needed for a certain command
 * author: Adam Debbiche
 */
int countPipesNeeded(Pgm *p){
  int pipeNumbers = -1;
  Pgm *pCopy = p;
  while(pCopy) {
    pipeNumbers++;
    pCopy = pCopy -> next;
  }
  return pipeNumbers;
}


/*
 * Recursive function that
 * loops through the commands and 
 * spawns child processes for each pipe
 * author: Adam Debbiche
 */
void handle_pipe(Pgm *p, int pipes[]){
  if (p == NULL) {
    return;
  }
  else {
    pip = pip + 2;
    int j;
    for(j = 0; j < pip; j += 2){	
	pipe(pipes + j);
      }
    handle_pipe(p -> next, pipes); 
    int pid = fork();
    
    if (pid == 0){ // child process code
      if (firstRun == 1) { // first run	
	if (stdinstr != NULL) {
	  FILE *fp = freopen (stdinstr, "r", stdin);
	}
	dup2(pipes[pCounter+1], 1);	
      }
      else if (firstRun == 0 && pCounter < pipeNumbers+1){ // not first run, there is previous command and a next one
	if (stdinstr != NULL) {
	  FILE *fp = freopen (stdinstr, "r", stdin);
	}
	dup2(pipes[pCounter-2], 0);
	dup2(pipes[pCounter+1], 1);
      }
      else if (firstRun == 0 && pCounter >= pipeNumbers+1 ){ // last command to run, there is only previous, no next one{
	dup2(pipes[pCounter-2], 0);
	if (stdoutstr != NULL){
	  printf("We have stdout: %s \n", stdoutstr);
	  int fd = open(stdoutstr, O_WRONLY|O_CREAT|O_TRUNC);
	  dup2(fd, 1);
	}
      }
      
      char **pl = p->pgmlist; 
      char *bin = search_path(*pl);            
      execvp(bin, pl);

    } // end child process code
    else { // parent code 
      if (firstRun == 1) {
	close(pipes[pCounter+1]);
	firstRun = 0;
      } 
      else if (firstRun == 0 &&  pCounter < pipeNumbers+1) {
	close(pipes[pCounter+1]);
	close(pipes[pCounter-2]);
      }
      else if (firstRun == 0 && pCounter >= pipeNumbers+1){
	close(pipes[pCounter-2]);	     
      }
      int status;
      wait(&status);
    } // end parent code
    pCounter = pCounter+2;
  } 
}


