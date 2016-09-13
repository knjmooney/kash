/* stack holds void addresses               */
/* functions haven't been tested rigorously */

#ifndef STACK_H
#define STACK_H

typedef struct {
  void **items;
  int size;
  int max_size;
} STACK;


STACK * init_stack(int max);

void free_stack(STACK * s);

int isEmpty(STACK * s);

void push(void * p, STACK * s);

void * pop(STACK * s);

#endif
