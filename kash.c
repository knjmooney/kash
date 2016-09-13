/**********************************************************************
 * Name   : kash.c
 * Author : Kevin Mooney
 * Date   : 29/03/16
 *
 * An interactive c-shell
 *
 * NOTES:
 * Executes anything in path
 * Can accept wildcards
 * Implemented my own string parser rather than strtok or strsep
 * Bit messy overall, could consider making seperate files
 * A list would be better than a stack for keeping track of arguments
 * Can redirection to files or multiple pipes
 * Can run jobs in the background
 *
 * TASKS:
 * prompt       : DONE, possibly should just print dir
 * cd           : DONE
 * exec         : DONE, currently reads from global PATH
 * redirect     : DONE, scope for more redirects
 * pipe         : DONE, need to handle closing of fds better
 * background   : DONE
 * groups       : 
 *
 * TODO:
 * Check for memory leaks
 * Proper error checking with perror 
 * Fix closing of fds in execute
 * Is it _exit or exit?
 * Kill signals
 **********************************************************************/

#include <stdio.h>		/* this needs to be defined before readline */
#include <readline/readline.h>	/*  */
#include <readline/history.h>

#include <stdlib.h>		/* malloc and free */
#include <string.h>		/* general string functions */

#include <errno.h>		/* perror */
#include <fcntl.h>		/* open and close */
#include <pwd.h>		/* get pwd */
#include <sys/types.h>		/* pid_t? */
#include <sys/stat.h>		/* File permissions */
#include <sys/wait.h>		/* wait_pid */
#include <unistd.h>		/* POSIX functions */
#include <wordexp.h>		/* tilde and wildcards */

#include "stack.h"		/* basic stack implementation */
#include "procs.h"		/* process utiliies */

/* Some definitions and global variables */
#define BUFFERSIZE 500

char *HOME;
char  OLDPWD[BUFFERSIZE];
char *PWD;

/* Prints a welcome message, Might be better to store in a seperate file */
void print_welcome() {

  printf("\033[01;34m");
  printf("\n");
  printf("                              /$$             \n");
  printf("       /$$   /$$  /$$$$$$   /$$$$$$  /$$   /$$\n");
  printf("      | $$  /$$/ /$$__  $$ /$$__  $$| $$  | $$\n");
  printf("      | $$ /$$/ | $$  \\ $$| $$  \\__/| $$  | $$\n");
  printf("      | $$$$$/  | $$$$$$$$|  $$$$$$ | $$$$$$$$\n"); 
  printf("      | $$  $$  | $$__  $$ \\____  $$| $$__  $$\n");
  printf("      | $$\\  $$ | $$  | $$ /$$  \\ $$| $$  | $$\n"); 
  printf("      | $$ \\  $$| $$  | $$|  $$$$$$/| $$  | $$\n");
  printf("      |__/  \\__/|__/  |__/ \\_  $$_/ |__/  |__/\n");
  printf("                             \\__/             \n");
  printf("\n");                         

  printf("\033[00m");
}

/* Gets rid of trailing whitespace                 */
/* TODO: write an opposite function ( efficiency ) */
void decr(char *str) {
  while ( *str != '\0' ) {
    str++;
  }
  str--;
  while ( *str == ' ' ) {
    *str = '\0';
    str--;
  }
}

/* will move *str back to the next double quote         */
/* TODO: Check for escape characters ( error checking ) */
int find_next_double_quote( char **str , char *start ) {
  if ( *str == start ) {
    fprintf (stderr,"Expected another \"\n");
    return 0;
  }

  --(*str);  
  while ( **str != '"' ) {
    if ( *str == start ) {
      fprintf (stderr,"Expected another \"\n");
      return 0;
    }
    --(*str);    
  }
  return 1;
}

/* will move *str back to the next ' */
/* TODO: Same as above */
int find_next_single_quote( char **str , char *start ) {
  if ( *str == start ) {
    fprintf (stderr,"Expected another \'\n"); /* set errno instead, maybe */
    return 0;
  }

  --(*str);  
  while ( **str != '\'' ) {
    if ( *str == start ) {
      fprintf (stderr,"Expected another \'\n");
      return 0;
    }
    --(*str);    
  }
  return 1;
}

/* returns true if revious character in string is c */
int is_previous_char(char c, char * pos, char * start) {
  if ( pos == start )
    return 0;
  if ( pos[-1] == c )
    return 1;
  return 0;
}

/* Uses wordexp to expand wildcards and do tilde expansion */
/* TODO: rename variables ( readability ) */
char ** expand_wildcards(STACK *stk) {
  
  wordexp_t p;
  char    **args, *word, **w;
  int       i, j;

  args = (char **) malloc ( BUFFERSIZE * sizeof (char *) ); 

  i=0;
  word = (char *) pop(stk);
  args[i] = (char *) malloc( BUFFERSIZE * sizeof(char) );
  strcpy( args[i++], word );

  while ( !isEmpty(stk) ) {
    word = (char *) pop(stk);
    
    wordexp(word, &p, 0);
    w = p.we_wordv;
    for (j = 0; j < p.we_wordc; j++) {
      args[i] = (char *) malloc( BUFFERSIZE * sizeof(char) );
      strcpy( args[i++], w[j] );
    }
    wordfree(&p);
  }
  args[i] = (char *) NULL;

  return args;
}

/* Changes directory to dir, if dir is NULL, changes to home */
void cd(char *dir) {
  if ( dir != NULL ) {
    if ( chdir( dir ) == 0 ) {
      strcpy ( OLDPWD, PWD );
    } else if ( strcmp ( dir, "-" ) == 0 ) {
      chdir ( OLDPWD );
      strcpy ( OLDPWD, PWD );
    } else {
      perror("cd");	    
    }
  } else {
    chdir ( HOME );
    strcpy ( OLDPWD, PWD );
  }
}

/* Wraps around system calls */
/* Currently only supports chdir */
int system_call ( char ** args ) {
  if ( strcmp ( "cd", args[0] ) == 0 ) {
    cd ( args[1] );
    return 1;
  }
  return 0;
}

/* Executes all processes in current session*? */
/* The argument passed is a stack of PROCESS struct */
/* Currently the method for closing fds is overcompensating */
void execute(STACK *processes) {
  PROCESS * proc = NULL;
  char    **args;
  pid_t     pid;
  int       nprocs;
  int       i;
  int       stat;

  nprocs = processes->size;

  /* While there are still processes on the stack */
  while ( ! isEmpty(processes) ) {
    proc = (PROCESS *) pop(processes);
    args = expand_wildcards(proc->stk);

    /* set group here maybe ? */

    /* Tries a direct system call, if not, forks */
    if ( ! system_call ( args ) ) {

#ifdef DEBUG
      printf ( "fin: %d, fout: %d, ferr: %d, pipe in: %d pipe out: %d\n"
      	       ,proc->fin,proc->fout,proc->ferr,proc->pipein_end,proc->pipeout_end);
      for ( i=0; args[i] != NULL; i++ ) {
      	printf("%s ",args[i]);
      }
      printf("\n");
#endif
      
      pid = fork();
      switch(pid) {
      case -1:			/* failed */
	perror(args[0]);
	break;
    
      case 0:			/* child */
	if ( dup2 ( proc->fin,   STDIN_FILENO  ) == -1 ) { perror("fin" ); }
	if ( dup2 ( proc->fout,  STDOUT_FILENO ) == -1 ) { perror("fout"); }
	if ( dup2 ( proc->ferr,  STDERR_FILENO ) == -1 ) { perror("ferr"); }

	/* Need to redo this */
	close_fds ( proc );
	if ( proc->pipein_end    > 2 ) close ( proc->pipein_end );

	/* Look in PATH */
	execvp( args[0], args );

	/* Alternative, complains about permissions */
	/* char PATH[BUFFERSIZE] */
	/* execv ( "/bin", args ); */
	/* execv ( "/usr/bin", args ); */
	/* snprintf ( PATH, sizeof(PATH), "%s/bin", HOME ); */
	/* execv ( PATH, args ); */

	perror( args[0] );
	_exit ( errno );	/* not sure about this */
      default:			/* Parent */

	close_fds ( proc );
	for ( i=0; args[i] != NULL; i++ )
	  free ( args[i] );
	
	break;
      }
    }
  }

  /* close fds */
  close_fds ( proc );
  if ( proc->pipein_end    > 2 ) close ( proc->pipein_end );
 
  /* wait til all processes have exited */
  for ( i=0; i<nprocs; i++ )
    waitpid(0, &stat, WUNTRACED);
}

/* String parser, seperates each pipe onto a stack */
void parse ( char * cmd ) {
  char *copy, *start, *file;
  STACK *processes;
  PROCESS * proc;
  pid_t pid;
  int L;
  int pipefd[2];
  
  /* Initialise memory */
  proc  = init_proc(BUFFERSIZE);
  copy  = malloc    ( BUFFERSIZE * sizeof(char   ) );
  processes = init_stack( BUFFERSIZE ); 
  
  L = strlen( cmd );
  if ( L >= BUFFERSIZE ) {
    fprintf(stderr,"Command too long to parse\n");
    return;
  }
  
  /* copy command before parsing */
  strcpy( copy, cmd );
  decr  ( copy );  		/* get rid of trailing whitespace */
  
  /* Move to end of command and parse right to left */
  start = copy;
  copy += L;
  while ( copy != start ) {
    --copy;

    /* If there's still a command, push it onto the stack */
    if ( copy == start && *copy != '\0' ) {
      push( copy, proc->stk );
    }
    
    switch ( *copy ) {

    case ' ':		/* get rid of spaces */
      push(copy+1,proc->stk);
      *copy = '\0';
      decr(start);
      break;

    case '\"':		/* double quote */
      if ( !find_next_double_quote( &copy, start) )
	return;
      break;

    case '\'':		/* single quote */
      if ( !find_next_single_quote( &copy, start) )
	return;
      break;
      
    case '<':		/* read from file */
      if ( copy[1] != '\0' ) {  file = &copy[1];                 }
      else                   {  file = (char *) pop(proc->stk);  }
      proc->fin   = open ( file, O_RDONLY );
      if ( proc->fin < 0 ) {
	perror( file );
	return;
      }
      *copy = '\0';
      break;
      
    case '>':		/* print to file */
     
      if ( copy[1] != '\0' ) {	file = &copy[1];                 }
      else	             {  file = (char *) pop(proc->stk);  }

      if ( is_previous_char('>',copy,start) ) { /* append stdout */
	proc->fout  = open ( file, O_WRONLY | O_CREAT | O_APPEND, 0664 );
	*copy = '\0'; --copy;
      }
      else if ( is_previous_char('2',copy,start) ) { /* truncate stderr */
	proc->ferr  = open ( file, O_WRONLY | O_CREAT | O_TRUNC, 0664 );
	*copy = '\0'; --copy;
      }
      else			/* truncate stderr */
	proc->fout  = open ( file, O_WRONLY | O_CREAT | O_TRUNC, 0664 );

      if ( proc->fout < 0 ) {
	perror( file );
	return;
      }
      *copy = '\0';
      break;
      
    case '|':		/* Pipe */
      *copy = '\0';
      if ( copy[1] != '\0' ) push( &copy[1], proc->stk );

      if (pipe(pipefd) == -1) {
	perror("pipe");
	exit(EXIT_FAILURE);
      }

      if ( isEmpty ( proc->stk ) ) {
	printf("Is empty\n");
	file = readline("> ");	  /* Need to free this */
	push ( file, proc->stk ); /* Need to reparse string */
      }

      replace_fd(&proc->fin,&pipefd[0]);
      proc->pipeout_end = pipefd[1];
      push ( proc, processes );
      
      proc = init_proc( BUFFERSIZE );
      replace_fd(&proc->fout,&pipefd[1]);
      proc->pipein_end = pipefd[0];
      break;
      
    case '&':			/* Run in background */
      *copy = '\0';

      if ( ( pid = fork() ) == -1 ) {
	perror("Background");
      } 
      else if ( pid == 0 ) {
	/* setpgid(0,0); */
	setsid();
	parse(start);
	exit(0);
      }
      else {
	printf("[ ] %d\n",pid);
	copy = start;
      }
      break;
    }
  }
  
  if ( isEmpty( proc->stk ) ) {
    return;
  }
  
  push ( proc, processes );
  
  execute(processes);

  /* NEED TO FREE RESOURCES */
  free ( copy );
  /* free_proc(proc); */
}

int main()
{
  char* input, shell_prompt[100];

  print_welcome();

  /* set_env_variables(); */
  HOME = getpwuid(getuid())->pw_dir;

  for(;;) {
    // Create prompt string from user name and current working directory.
    PWD = getcwd(NULL,1024);
    snprintf(shell_prompt, sizeof(shell_prompt), "\033[01;35m%s\033[00m$ ", PWD );

    // Display prompt and read input
    input = readline(shell_prompt);
    
    // Check for EOF.
    if (!input) {
      printf("\n");
      break;
    }

    /* Parse if not empty */
    if ( *input != '\0' ) {
      parse ( input );
      add_history(input); /* Add input to history. */
    }

    // Free strings
    free(PWD  );
    free(input);
  }

  return EXIT_SUCCESS;
}
