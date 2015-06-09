// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <alloca.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/scheduler.h>
#include "isir.h"

typedef struct {
  hpx_parcel_t *p;
  void *out;
  char data[];
} _isir_lco_get_reply_args_t;

static int
_isir_lco_get_reply_handler(_isir_lco_get_reply_args_t *args, size_t n) {
  size_t bytes = n - sizeof(*args);
  if (bytes) {
    memcpy(args->out, args->data, bytes);
  }
  scheduler_spawn(args->p);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_INTERRUPT, HPX_MARSHALLED, _isir_lco_get_reply,
                  _isir_lco_get_reply_handler, HPX_POINTER, HPX_SIZE_T);

static int
_isir_lco_get_handler(hpx_parcel_t *p, size_t n, void *out) {
  hpx_addr_t lco = hpx_thread_current_target();

  dbg_assert_str(n < here->config->stacksize,
                 "remote lco get could overflow stack\n");
  _isir_lco_get_reply_args_t *args = alloca(sizeof(*args) + n);
  dbg_assert(args);
  args->p = p;
  args->out = out;

  int e;
  // convert to wait if there's no buffer
  if (n == 0) {
    e = hpx_lco_wait(lco);
  }
  else {
    e = hpx_lco_get(lco, n, args->data);
  }
  dbg_check(e, "Failure in remote get operation\n");

  hpx_thread_continue(args, sizeof(*args) + n);
}
static HPX_ACTION(HPX_DEFAULT, 0, _isir_lco_get, _isir_lco_get_handler,
                  HPX_POINTER, HPX_SIZE_T, HPX_POINTER);

int
isir_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out) {
  hpx_parcel_t *current = scheduler_current_parcel();
  hpx_parcel_t *p = parcel_create(lco, _isir_lco_get, HPX_HERE,
                                  _isir_lco_get_reply, 3, &current, &n, &out);
  return scheduler_wait_launch_through(p, lco);
}
