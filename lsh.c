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
int firstRun = 1; // boolean for checking if we are running the first cmd of a pipe
int pCounter = 0; // pipe counter
int pipeNumbers; // numbers of pipes we need
int pip; // keeps count of pipes created
char *stdinstr; // copy of stdin of cmd
char *stdoutstr; // copy of stdout of cmd

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
	run_command(&cmd); // runs the command entered
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
 * Runs a command the user entered 
 * on the terminal
 * author: Adam Debbiche
 */
void run_command(Command *cmd){
  Pgm *p = cmd->pgm;
  char **pl = p->pgmlist;
  stdinstr = cmd->rstdin;
  stdoutstr = cmd->rstdout;
  sigset_t mask, omask;

  // register signals 
  signal(SIGINT, signal_handler);
  if (cmd -> bakground == 1) 
    signal(SIGCHLD, signal_handler);
  
  
 
  // Here we first check if the command has any pipes 
  // if yes then handle them
  if (p->next != NULL) {
    pipeNumbers = countPipesNeeded(p);
    pCounter = 0;
    pip = 0;
    firstRun = 1;
    int pipes[pipeNumbers*2];
    handle_pipe(p, pipes);
  } else {
   
    // quit if user enters "exit"
    if (strcmp(*pl,"exit") == 0)
      exit(0);

    // change dir if user enter "cd"
    if (strcmp(*pl,"cd") == 0) {
      change_dir(pl);
      return;
    }
    
    int status;  
    char *prg = search_path(*pl); // search for cmd bin
    if (prg == NULL) {
	printf("%s: command not found \n", *pl);
	return;
    }
    
    // fork a child process to run cmd
    int pid = fork(); 
    // child process
    if (pid == 0) {
      if(cmd->bakground)
        signal(SIGINT, SIG_IGN);
      else
	signal(SIGINT, SIG_DFL);
      
      // check if we have stdin
      if (cmd -> rstdin != NULL) {
	FILE *fp;
	// redirect stdin
	fp = freopen (cmd ->rstdin, "r", stdin);
       }
      
      // redirect stdout if we have one
      if (cmd->rstdout != NULL){
        mode_t mode =  S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
	int fd = creat(cmd->rstdout,mode);
	dup2(fd, 1);
        close(fd);
      }
      execvp(prg, pl); // run command
    }
    
    // parent code, just waiting for child
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
  char *envvar = getenv("PATH"); // get PATH variable
  size_t length  = strlen (envvar)+1;
  char * pathvar = malloc (length); // allocate memory for array
  strcpy (pathvar, envvar); 
  char *path_tokens = strtok(pathvar, ":");
  
  while(path_tokens != NULL) {
    struct stat info;
    // allocate mem for concatenated string
    char *fullpath = malloc(strlen(path_tokens)+
			    strlen(executable)+1);
    strcpy(fullpath, path_tokens);
    strcat(fullpath, "/"); // we need to add a /
    strcat(fullpath, executable);

    if (stat(fullpath, &info) == 0){ // fails if file doesn't exist 
      return fullpath;
    }
    path_tokens = strtok(NULL, ":");
  }
  free(pathvar);
  return NULL; // command not found!
}

/*
 * Implements the cd command of our shell
 * author: Adam Debbiche
 */
void change_dir(char **pl){
  *pl++;
  // change directory
  chdir(*pl);
}

/*
 * Handles the interruption signal of SIGINT
 * when Ctrl-C is pressed
 * author: Adam Debbiche
 */
void signal_handler(int signal){
  int status;
  if (signal == SIGINT) return; 
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
  // base case
  if (p == NULL) {
    return; 
  }
  else {
    pip = pip + 2;
    int j;
    // create the pipes needed
    for(j = 0; j < pip; j += 2){	
	pipe(pipes + j);
      }
    handle_pipe(p -> next, pipes); 
    
    int pid = fork();
    // child code
    if (pid == 0){
      
      // first run (running first command)
      if (firstRun == 1) {
	// redirect stdin if needed
	if (stdinstr != NULL) {
	  FILE *fp = freopen (stdinstr, "r", stdin);
	}
	dup2(pipes[pCounter+1], 1);	
      }
      
      // not first run, there is previous command and a next one
      if (firstRun == 0 && (pCounter/pipeNumbers != 2)){ 
	// redirect stdin if needed
	if (stdinstr != NULL) {
	  FILE *fp = freopen (stdinstr, "r", stdin);
	}
	dup2(pipes[pCounter-2], 0); // redirect input
	dup2(pipes[pCounter+1], 1); // redirect output
      }

      // last command to run, there is only previous, no next one
      if (firstRun == 0 && (pCounter/pipeNumbers == 2)){ 
	dup2(pipes[pCounter-2], 0);
	// redirect stdout if needed
	if (stdoutstr != NULL){
	  mode_t mode =  S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
	  int fd = creat(stdoutstr,mode);
	  dup2(fd, 1);
	}
      }
      // search for binary then execute it
      char **pl = p->pgmlist; 
      char *bin = search_path(*pl);            
      execvp(bin, pl);

    } // end child process code
    // parent code, just closing its end of pipes then waiting
    else {
      // first command
      if (firstRun == 1) {
	close(pipes[pCounter+1]);
	firstRun = 0;
      } 
      // running command with a previous one and a next one
      else if (firstRun == 0 &&  pCounter < pipeNumbers+1) {
	close(pipes[pCounter+1]);
	close(pipes[pCounter-2]);
      }
      // last command 
      else if (firstRun == 0 && pCounter >= pipeNumbers+1){
	close(pipes[pCounter-2]);
	close(pipes[pCounter+1]);
      }
      int status;
      wait(&status); // wait for child
    } // end parent code
    pCounter = pCounter+2; // increase the pipe counter
  } 
}


