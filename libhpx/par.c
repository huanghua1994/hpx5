// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/par.c
/// @brief Implements the HPX par constructs
///

#include <stdlib.h>
#include <string.h>
#include "libhpx/action.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "hpx/hpx.h"


static int _par_for_async_action(hpx_for_action_t f, void *args, int min, int max) {
  for (int i = min, e = max; i < e; ++i) {
    f(i, args);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(DEFAULT, _par_for_async_action, _par_for_async,
                      HPX_POINTER, HPX_POINTER, HPX_INT, HPX_INT);

int hpx_par_for(hpx_for_action_t f, const int min, const int max,
                const void *args, hpx_addr_t sync) {
  dbg_assert(max - min > 0);

  // get the number of scheduler threads
  int nthreads = HPX_THREADS;

  hpx_addr_t and = HPX_NULL;
  if (sync) {
    and = hpx_lco_and_new(nthreads);
    hpx_call_when_with_continuation(and, sync, hpx_lco_set_action,
                                    and, hpx_lco_delete_action, NULL, 0);
  }

  const int n = max - min;
  const int m = n / nthreads;
  int r = n % nthreads;
  int rmin = min;
  int rmax = max;

  int base = rmin;
  for (int i = 0, e = nthreads; i < e; ++i) {
    rmin = base;
    rmax = base + m + ((r-- > 0) ? 1 : 0);
    base = rmax;
    int e = hpx_call(HPX_HERE, _par_for_async, and, &f, &args, &rmin, &rmax);
    if (e) {
      return e;
    }
  }

  return HPX_SUCCESS;
}

int hpx_par_for_sync(hpx_for_action_t f, const int min, const int max,
                     const void *args) {
  dbg_assert(max - min > 0);
  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    return log_error("could not allocate an LCO.\n");
  }

  int e = hpx_par_for(f, min, max, args, sync);
  if (!e) {
    e = hpx_lco_wait(sync);
  }
  hpx_lco_delete(sync, HPX_NULL);
  return e;
}


/// HPX parallel "call".

typedef struct {
  hpx_action_t action;
  int min;
  int max;
  int branching_factor;
  int cutoff;
  size_t arg_size;
  void (*arg_init)(void*, const int, const void*);
  hpx_addr_t sync;
  char env[];
} par_call_async_args_t;

static HPX_ACTION(_par_call_async, par_call_async_args_t *args) {
  const size_t env_size = hpx_thread_current_args_size() - sizeof(*args);
  return hpx_par_call(args->action, args->min, args->max, args->branching_factor, args->cutoff,
                      args->arg_size, args->arg_init, env_size, &args->env, args->sync);
}

int hpx_par_call(hpx_action_t action, const int min, const int max,
                 const int branching_factor,
                 const int cutoff,
                 const size_t arg_size,
                 void (*arg_init)(void*, const int, const void*),
                 const size_t env_size, const void *env,
                 hpx_addr_t sync) {
  dbg_assert(max - min > 0);
  dbg_assert(branching_factor > 0);
  dbg_assert(cutoff > 0);

  hpx_addr_t and = HPX_NULL;
  if (sync) {
    and = hpx_lco_and_new(max - min);
    hpx_call_when_with_continuation(and, sync, hpx_lco_set_action,
                                    and, hpx_lco_delete_action, NULL, 0);
  }
  
  const int n = max - min;

  // if we're still doing divide and conquer, then do it
  if (n > cutoff) {
    const int m = n / branching_factor;
    int r = n % branching_factor;

    // we'll reuse this buffer
    par_call_async_args_t *args = malloc(sizeof(par_call_async_args_t) + env_size);
    args->action = action;
    args->min = min;
    args->max = min + m + ((r-- > 0) ? 1 : 0);
    args->cutoff = cutoff;
    args->branching_factor = branching_factor;
    args->arg_size = arg_size;
    args->arg_init = arg_init;
    args->sync = and;
    memcpy(&args->env, env, env_size);

    for (int i = 0, e = branching_factor; i < e; ++i) {
      int e = hpx_call(HPX_HERE, _par_call_async, HPX_NULL, args, sizeof(*args) + env_size);
      if (e) {
        return e;
      }

      // update the buffer to resend
      args->min = args->max;
      args->max = args->max + m + ((r-- > 0) ? 1 : 0);

      if (args->max <= args->min) {
        return HPX_SUCCESS;
      }
    }
  }
  else {
    // otherwise we're in the cutoff region, do the actions sequentially
    for (int i = min, e = max; i < e; ++i) {
      hpx_parcel_t *p = hpx_parcel_acquire(NULL, arg_size);
      hpx_parcel_set_action(p, action);
      hpx_parcel_set_cont_action(p, hpx_lco_set_action);
      hpx_parcel_set_cont_target(p, and);
      if (arg_init) {
        arg_init(hpx_parcel_get_data(p), i, env);
      }
      hpx_parcel_send(p, HPX_NULL);
    }
  }

  return HPX_SUCCESS;
}

int hpx_par_call_sync(hpx_action_t action,
                      const int min, const int max,
                      const int branching_factor,
                      const int cutoff,
                      const size_t arg_size,
                      void (*arg_init)(void*, const int, const void*),
                      const size_t env_size, const void *env) {
  assert(max - min > 0);
  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    return log_error("could not allocate an LCO.\n");
  }

  int e = hpx_par_call(action, min, max, branching_factor, cutoff, arg_size,
                       arg_init, env_size, env, sync);
  if (!e) {
    e = hpx_lco_wait(sync);
  }
  hpx_lco_delete(sync, HPX_NULL);
  return e;
}

typedef struct {
  hpx_action_t action;
  hpx_addr_t addr;
  size_t count;
  size_t increment;
  size_t bsize;
  size_t arg_size;
  char arg[];
} hpx_count_range_call_args_t;

static HPX_ACTION(_hpx_count_range_call, const hpx_count_range_call_args_t *const args) {
  int status;
  for (size_t i = 0; i < args->count; ++i) {
    const hpx_addr_t target =
      hpx_addr_add(args->addr, i * args->increment, args->bsize);
    status = hpx_call(target, args->action, HPX_NULL, args->arg, args->arg_size);
    if (status != HPX_SUCCESS) {
      return status;
    }
  }

  return HPX_SUCCESS;
}

int hpx_count_range_call(hpx_action_t action,
                         const hpx_addr_t addr,
                         const size_t count,
                         const size_t increment,
                         const uint32_t bsize,
                         const size_t arg_size,
                         void *const arg) {
  const size_t thread_chunk = count / (HPX_LOCALITIES * HPX_THREADS);
  hpx_count_range_call_args_t *args = malloc(sizeof(*args) + arg_size);
  memcpy(args->arg, arg, arg_size);
  args->action = action; args->count = thread_chunk;
  args->increment = increment; args->bsize = bsize; args->arg_size = arg_size;
  for (size_t l = 0; l < HPX_LOCALITIES; ++l) {
    for (size_t t = 0; t < HPX_THREADS; ++t) {
      const uint64_t addr_delta =
        (l * HPX_THREADS + t) * thread_chunk * increment;
      args->addr = hpx_addr_add(addr, addr_delta, bsize);
      hpx_call(HPX_THERE(l), _hpx_count_range_call, HPX_NULL, args,
               sizeof(*args) + arg_size);
    }
  }
  args->count = count % (HPX_LOCALITIES * HPX_THREADS);
  const uint64_t addr_delta =
    HPX_LOCALITIES * HPX_THREADS * thread_chunk * increment;
  args->addr = hpx_addr_add(addr, addr_delta, bsize);
  hpx_call(HPX_HERE, _hpx_count_range_call, HPX_NULL, args,
           sizeof(*args) + arg_size);
  free(args);
  return HPX_SUCCESS;
}
