/* stack holds void addresses               */
/* functions haven't been tested rigorously */

#include <stdio.h>
#include <stdlib.h>

#include "stack.h"

STACK * init_stack(int max) {
  STACK * s = malloc( sizeof(STACK));
  s->items  = calloc( max, sizeof(void *) ); 
  s->size = 0;
  s->max_size = max;

  return s;
}

void free_stack(STACK *s) {
  free(s->items);
  free(s);
}

int isEmpty(STACK * s) {
  return s->size == 0; 
}

void push(void * p, STACK * s) {
  if(s->size >= s->max_size) {
    fprintf(stderr,"Stack overflow\n");
    return;
  }
  s->items[s->size] = p; 
  s->size++;
}

void * pop(STACK * s) {
  if(isEmpty(s)) {
    fprintf(stderr,"Can't pop empty stack\n");
    return NULL;
  }
  s->size--;
  return s->items[s->size]; 
}
