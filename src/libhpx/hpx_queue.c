
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Priority Queue Functions
  hpx_queue.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#include <stdlib.h>
#include <sys/queue.h>
#include "hpx_queue.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_queue_init

  Initialize a priority queue.  This function should be called
  before using any other functions on this queue.
 --------------------------------------------------------------------
*/

void hpx_queue_init(hpx_queue_t * q) {  
  q->head = NULL;
  q->tail = NULL;
  q->count = 0;
}


/*
 --------------------------------------------------------------------
  hpx_queue_destroy

  Frees any memory allocated by this queue.  Should be called after
  all other functions.
 --------------------------------------------------------------------
*/

void hpx_queue_destroy(hpx_queue_t * q) {
  hpx_queue_node_t * cur = NULL;
  hpx_queue_node_t * next = NULL;
  
  cur = q->head;
  while (q->count > 0) {
    next = cur->next;
    hpx_free(cur);
    cur = next;

    q->count -= 1;
  }

  q->head = NULL;
  q->tail = NULL;
}


/*
 --------------------------------------------------------------------
  hpx_queue_size

  Returns the number of elements in the queue.
 --------------------------------------------------------------------
*/

uint64_t hpx_queue_size(hpx_queue_t * q) {
  return q->count;
}


/*
 --------------------------------------------------------------------
  hpx_queue_peek

  Returns the front element WITHOUT popping it off of the queue.
 --------------------------------------------------------------------
*/

void * hpx_queue_peek(hpx_queue_t * q) {
  hpx_queue_node_t * node;
  void * val = NULL;

  node = q->head;
  if (node != NULL) {
    val = node->value;
  }
  
  return val;
}


/*
 --------------------------------------------------------------------
  hpx_queue_push

  Push an element into the back of the queue.
 --------------------------------------------------------------------
*/

void hpx_queue_push(hpx_queue_t * q, void * val) {
  struct _hpx_queue_node_t * node = NULL;

  node = (struct _hpx_queue_node_t *) hpx_alloc(sizeof(struct _hpx_queue_node_t));
  if (node != NULL) {
    node->value = val;
    node->next = NULL;

    if (q->count == 0) {
      q->head = node;
    } else {
      q->tail->next = node;
    }

    q->tail = node;
    q->count += 1;
  }
}


/*
 --------------------------------------------------------------------
  hpx_queue_pop

  Pops the front element off of the queue and returns it.
 --------------------------------------------------------------------
*/

void * hpx_queue_pop(hpx_queue_t * q) {
  hpx_queue_node_t * node = NULL;
  void * val = NULL;

  node = q->head;
  if (node != NULL) {
    val = node->value;

    q->head = q->head->next;
    hpx_free(node);    

    if (q->count == 1) {
      q->tail = NULL;
    }

    q->count -= 1;
  }

  return val;
}
