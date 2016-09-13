typedef struct {
  STACK *stk;
  int    fin;
  int    fout;
  int    ferr;
  int    pipein_end;
  int    pipeout_end;
} PROCESS;

PROCESS * init_proc(int buffersize) { 
  PROCESS * proc = (PROCESS *) malloc( sizeof(PROCESS) );

  proc->stk         = init_stack( buffersize );
  proc->fin         = STDIN_FILENO;
  proc->fout        = STDOUT_FILENO;
  proc->ferr        = STDERR_FILENO; 
  proc->pipein_end  = -1;
  proc->pipeout_end = -1;

  return proc;
}

void replace_fd(int *fdnew, int *fdold) {
  if ( *fdnew > STDERR_FILENO ) {
    close ( *fdnew );
  }
  *fdnew = *fdold;
}

void close_fds(PROCESS * proc) {
  if ( proc->fin         > 2 ) close (proc->fin);
  if ( proc->fout        > 2 ) close (proc->fout);
  if ( proc->ferr        > 2 ) close (proc->ferr);
  /* if ( proc->pipein_end  > 2 ) close (proc->pipein_end); */
  if ( proc->pipeout_end > 2 ) close (proc->pipeout_end);
}

void free_proc(PROCESS * proc) {
  fprintf(stderr,"free_proc has not been implemented\n");
} 
