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
# include "config.h"
#endif

/// @file libhpx/call.c
/// @brief Implement the hpx/call.h header.
///

#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

static hpx_action_t _bcast = 0;


typedef struct {
  hpx_action_t action;
  char *data[];
} _bcast_args_t;


static int _bcast_action(_bcast_args_t *args) {
  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  uint32_t len = hpx_thread_current_args_size() - sizeof(args->action);
  int i, e;
  for (i = 0, e = here->ranks; i < e; ++i)
    hpx_call(HPX_THERE(i), args->action, args->data, len, and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return HPX_SUCCESS;
}


static HPX_CONSTRUCTOR void _init_actions(void) {
  _bcast = HPX_REGISTER_ACTION(_bcast_action);
}



/// A RPC call with a user-specified continuation action.
int
hpx_call_with_continuation(hpx_addr_t addr, hpx_action_t action,
                           const void *args, size_t len,
                           hpx_addr_t c_target, hpx_action_t c_action)
{
/*
#ifdef ENABLE_TAU
          TAU_START("hpx_call_with_continuation");
#endif
*/
  hpx_parcel_t *p = parcel_create(addr, action, args, len, c_target, c_action,
                                  hpx_thread_current_pid(), true);
  if (!p)
    return dbg_error("rpc: failed to create parcel.\n");

  hpx_parcel_send_sync(p);
/*
#ifdef ENABLE_TAU
          TAU_STOP("hpx_call_with_continuation");
#endif
*/
  return HPX_SUCCESS;
}

/// Encapsulates an asynchronous remote-procedure-call.
int
hpx_call(hpx_addr_t addr, hpx_action_t action, const void *args,
         size_t len, hpx_addr_t result)
{
  return hpx_call_with_continuation(addr, action, args, len, result,
                                    hpx_lco_set_action);
}


int
hpx_call_sync(hpx_addr_t addr, hpx_action_t action,
              const void *args, size_t alen,
              void *out, size_t olen)
{
/*
#ifdef ENABLE_TAU
          TAU_START("hpx_call_sync");
#endif
*/
  hpx_addr_t result = hpx_lco_future_new(olen);
  hpx_call(addr, action, args, alen, result);
  int status = hpx_lco_get(result, olen, out);
  hpx_lco_delete(result, HPX_NULL);
/*
#ifdef ENABLE_TAU
          TAU_STOP("hpx_call_sync");
#endif
*/
  return status;
}


int
hpx_call_async(hpx_addr_t addr, hpx_action_t action,
               const void *args, size_t len,
               hpx_addr_t args_reuse, hpx_addr_t result)
{
/*
#ifdef ENABLE_TAU
          TAU_START("hpx_call_async");
#endif
*/
  hpx_parcel_t *p =
      parcel_create(addr, action, args, len, result, hpx_lco_set_action,
                    hpx_thread_current_pid(), false);
  if (!p)
    return dbg_error("rpc: failed to create parcel.\n");

  hpx_parcel_send(p, args_reuse);
/*
#ifdef ENABLE_TAU
          TAU_STOP("hpx_call_async");
#endif
*/
  return HPX_SUCCESS;
}


/// Encapsulates a RPC called on all available localities.
int
hpx_bcast(hpx_action_t action, const void *data, size_t len, hpx_addr_t lco) {

/*
#ifdef ENABLE_TAU
          TAU_START("hpx_bcast");
#endif
*/  
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, len + sizeof(_bcast_args_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _bcast);
  hpx_parcel_set_cont_action(p, hpx_lco_set_action);
  hpx_parcel_set_cont_target(p, lco);

  _bcast_args_t *args = (_bcast_args_t *)hpx_parcel_get_data(p);
  args->action = action;
  memcpy(&args->data, data, len);

  hpx_parcel_send_sync(p);
/*
#ifdef ENABLE_TAU
          TAU_STOP("hpx_bcast");
#endif
*/
  return HPX_SUCCESS;
}
