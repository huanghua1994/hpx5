
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Thread Functions
  hpx_thread.c

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


#include <string.h>
#include "hpx_thread.h"
#include "hpx_error.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_thread_create

  Creates and initializes a thread.
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t * ctx, hpx_thread_func_t func, void * args) {
  hpx_thread_t * th = NULL;

  /* allocate the thread */
  th = (hpx_thread_t *) hpx_alloc(sizeof(hpx_thread_t));
  if (th != NULL) {
    /* initialize the thread */
    memset(th, 0, sizeof(hpx_thread_t));
    th->state = HPX_THREAD_STATE_PENDING;
    th->func = func;
    th->args = args;

    /* put the thread in the pending queue */
    /* this should really be the suspended queue, so this will change later */
    hpx_queue_push(&ctx->q_pend, th);
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  return th;
}


/*
 --------------------------------------------------------------------
  hpx_thread_destroy

  Cleans up a previously created thread.
 --------------------------------------------------------------------
*/

void hpx_thread_destroy(hpx_thread_t * th) {
  hpx_free(th);
}


/*
 --------------------------------------------------------------------
  hpx_thread_get_state

  Returns the queuing state of the thread.
 --------------------------------------------------------------------
*/

hpx_thread_state_t hpx_thread_get_state(hpx_thread_t * th) {
  return th->state;
}


/*
 --------------------------------------------------------------------
  hpx_thread_set_state

  Sets the queuing state of the thread.
 --------------------------------------------------------------------
*/

void hpx_thread_set_state(hpx_thread_t * th, hpx_thread_state_t state) {
  th->state = state;
}
