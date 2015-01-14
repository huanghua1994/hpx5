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


/// AlltoAll is an extention of allgather to the case where each process sends
/// distinct data to each of the receivers. The jth block sent from process i
/// is received by process j and is placed in the ith block of recvbuf.
/// (Complete exchange).
///
///           sendbuff
///           ####################################
///           #      #      #      #      #      #
///         0 #  A0  #  A1  #  A2  #  A3  #  A4  #
///           #      #      #      #      #      #
///           ####################################
///      T    #      #      #      #      #      #
///         1 #  B0  #  B1  #  B2  #  B3  #  B4  #
///      a    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         2 #  C0  #  C1  #  C2  #  C3  #  C4  #       BEFORE
///      k    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         3 #  D0  #  D1  #  D2  #  D3  #  D4  #
///           #      #      #      #      #      #
///           ####################################
///           #      #      #      #      #      #
///         4 #  E0  #  E1  #  E2  #  E3  #  E4  #
///           #      #      #      #      #      #
///           ####################################
///
///             <---------- recvbuff ---------->
///           ####################################
///           #      #      #      #      #      #
///         0 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################
///      T    #      #      #      #      #      #
///         1 #  A1  #  B1  #  C1  #  D1  #  E1  #
///      a    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         2 #  A2  #  B2  #  C2  #  D2  #  E2  #       AFTER
///      k    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         3 #  A3  #  B3  #  C3  #  D3  #  E3  #
///           #      #      #      #      #      #
///           ####################################
///           #      #      #      #      #      #
///         4 #  A4  #  B4  #  C4  #  D4  #  E4  #
///           #      #      #      #      #      #
///           ####################################

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/scheduler/allgather.c
/// @brief Defines the allgather LCO.
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

static const int _gathering = 0;
static const int _reading   = 1;

typedef struct {
  lco_t           lco;
  cvar_t         wait;
  size_t participants;
  size_t        count;
  volatile int  phase;
  void         *value;
} _alltoall_t;


typedef struct {
  int offset;
  char buffer[];
} _alltoall_set_offset_t;

typedef struct {
  int size;
  int offset;
} _alltoall_get_offset_t;

static hpx_action_t _alltoall_setid_action = 0;
static hpx_action_t _alltoall_getid_action = 0;

/// Deletes a gathering.
static void _alltoall_fini(lco_t *lco) {
  if (!lco)
    return;

  lco_lock(lco);
  _alltoall_t *g = (_alltoall_t *)lco;
  if (g->value)
    free(g->value);
  libhpx_global_free(g);
}


/// Handle an error condition.
static void _alltoall_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _alltoall_t *g = (_alltoall_t *)lco;
  scheduler_signal_error(&g->wait, code);
  lco_unlock(lco);
}

/// Get the value of the gathering, will wait if the phase is gathering.
static hpx_status_t _alltoall_getid(_alltoall_t *g, unsigned offset, int size,
                                    void *out) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're reading, and watch for errors
  while ((g->phase != _reading) && (status == HPX_SUCCESS))
    status = scheduler_wait(&g->lco.lock, &g->wait);

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS)
    goto unlock;

  // We're in the reading phase, if the user wants data copy it out
  if (size && out)
    memcpy(out, (char *)g->value + (offset * size), size);

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to gathering---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier _reading epochs
  if (++g->count == g->participants) {
    g->phase = _gathering;
    scheduler_signal_all(&g->wait);
  }
  else {
    while ((g->phase == _reading) && (status == HPX_SUCCESS))
      status = scheduler_wait(&g->lco.lock, &g->wait);
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

/// Get the ID for alltoall. This is global getid for the user to use.
/// Since the LCO is local, we use the local get functionality
///
/// @param   alltoall    Global address of the alltoall LCO
/// @param   id          The ID of our rank
/// @param   size        The size of the data being gathered
/// @param   value       Address of the value buffer
hpx_status_t hpx_lco_alltoall_getid(hpx_addr_t alltoall, unsigned id, int size,
                                    void *value) {
  hpx_status_t status = HPX_SUCCESS;
  _alltoall_t *local;

  if (!hpx_gas_try_pin(alltoall, (void**)&local)) {
    _alltoall_get_offset_t args = {.size = size, .offset = id};
    return hpx_call_sync(alltoall, _alltoall_getid_action, &args, sizeof(args),
                         value, size);
  }

  status = _alltoall_getid(local, id, size, value);
  hpx_gas_unpin(alltoall);
  return status;
}

static int _alltoall_getid_proxy(_alltoall_get_offset_t *args) {
  // try and pin the alltoall LCO, if we fail, we need to resend the underlying
  // parcel to "catch up" to the moving LCO
  hpx_addr_t target = hpx_thread_current_target();
  _alltoall_t *g;
  if(!hpx_gas_try_pin(target, (void **)&g))
     return HPX_RESEND;

  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local getid routine
  char buffer[args->size];
  hpx_status_t status = _alltoall_getid(g, args->offset, args->size, buffer);
  hpx_gas_unpin(target);

  // if success, finish the current thread's execution, sending buffer value to
  // the thread's continuation address else finish the current thread's execution.
  if(status == HPX_SUCCESS)
    hpx_thread_continue(args->size, buffer);
  else
    hpx_thread_exit(status);
}


// Wait for the gathering, loses the value of the gathering for this round.
static hpx_status_t _alltoall_wait(lco_t *lco) {
  _alltoall_t *g = (_alltoall_t *)lco;
  return _alltoall_getid(g, 0, 0, NULL);
}

// Local set id function.
static hpx_status_t _alltoall_setid(_alltoall_t *g, unsigned offset, int size,
                                    const void* buffer) {
  int nDoms;
  int elementSize;
  int columnOffset;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're gathering
  while ((g->phase != _gathering) && (status == HPX_SUCCESS))
    status = scheduler_wait(&g->lco.lock, &g->wait);

  if (status != HPX_SUCCESS)
    goto unlock;

  nDoms = g->participants;
  // copy in our chunk of the data
  assert(size && buffer);
  elementSize = size / nDoms;
  columnOffset = offset * elementSize;

  for (int i = 0; i < nDoms; i++)
  {
    int rowOffset = i * size;
    int tempOffset = rowOffset + columnOffset;
    int sourceOffset = i * elementSize;
    memcpy((char*)g->value + tempOffset, (char *)buffer + sourceOffset, elementSize);
  }

  // if we're the last one to arrive, switch the phase and signal readers
  if (--g->count == 0) {
    g->phase = _reading;
    scheduler_signal_all(&g->wait);
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

/// Set the ID for alltoall. This is global setid for the user to use.
///
/// @param   alltoall   Global address of the alltoall LCO
/// @param   id         ID to be set
/// @param   size       The size of the data being gathered
/// @param   value      Address of the value to be set
/// @param   lsync      An LCO to signal on local completion HPX_NULL if we
///                     don't care. Local completion indicates that the
///                     @value may be freed or reused.
/// @param   rsync      An LCO to signal remote completion HPX_NULL if we
///                     don't care.
/// @returns HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_alltoall_setid(hpx_addr_t alltoall, unsigned id, int size,
                                    const void *value, hpx_addr_t lsync,
                                    hpx_addr_t rsync) {
  hpx_status_t status = HPX_SUCCESS;
  _alltoall_t *local;

  if (!hpx_gas_try_pin(alltoall, (void**)&local)) {
    size_t args_size = sizeof(_alltoall_set_offset_t) + size;
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, args_size);
    assert(p);
    hpx_parcel_set_target(p, alltoall);
    hpx_parcel_set_action(p, _alltoall_setid_action);
    hpx_parcel_set_cont_target(p, rsync);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    _alltoall_set_offset_t *args = hpx_parcel_get_data(p);
    args->offset = id;
    memcpy(&args->buffer, value, size);
    hpx_parcel_send(p, lsync);
  }
  else {
    status = _alltoall_setid(local, id, size, value);
    if (lsync)
      hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    if (rsync)
      hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }

  return status;
}


static hpx_status_t _alltoall_setid_proxy(void *args) {
  // try and pin the allgather LCO, if we fail, we need to resend the underlying
  // parcel to "catch up" to the moving LCO
  hpx_addr_t target = hpx_thread_current_target();
  _alltoall_t *g;
  if(!hpx_gas_try_pin(target, (void **)&g))
     return HPX_RESEND;

  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local setid routine
  _alltoall_set_offset_t *a = args;
  size_t size = hpx_thread_current_args_size() - sizeof(_alltoall_set_offset_t);
  hpx_status_t status = _alltoall_setid(g, a->offset, size, &a->buffer);
  hpx_gas_unpin(target);
  return status;
}


static HPX_CONSTRUCTOR void _initialize_actions(void) {
  LIBHPX_REGISTER_ACTION(_alltoall_setid_proxy, &_alltoall_setid_action);
  LIBHPX_REGISTER_ACTION(_alltoall_getid_proxy, &_alltoall_getid_action);
}

static void _alltoall_set(lco_t *lco, int size, const void *from) {
  // can't call set on an alltoall
  hpx_abort();
}

static hpx_status_t _alltoall_get(lco_t *lco, int size, void *out) {
  // can't call get on an alltoall
  hpx_abort();
}

static void _alltoall_init(_alltoall_t *g, size_t participants, size_t size) {
  static const lco_class_t vtable = {
    _alltoall_fini,
    _alltoall_error,
    _alltoall_set,
    _alltoall_get,
    _alltoall_wait
  };

  lco_init(&g->lco, &vtable, 0);
  cvar_reset(&g->wait);
  g->participants = participants;
  g->count = participants;
  g->phase = _gathering;
  g->value = NULL;

  if (size) {
    // Ultimately, g->value points to start of the array containing the
    // scattered data.
    g->value = malloc(size * participants);
    assert(g->value);
  }
}

/// Allocate a new alltoall LCO. It scatters elements from each process in order
/// of their rank and sends the result to all the processes
///
/// The gathering is allocated in gathering-mode, i.e., it expects @p
/// participants to call the hpx_lco_alltoall_setid() operation as the first
/// phase of operation.
///
/// @param participants The static number of participants in the gathering.
/// @param size         The size of the data being gathered.
hpx_addr_t hpx_lco_alltoall_new(size_t inputs, size_t size) {
  _alltoall_t *g = libhpx_global_malloc(sizeof(*g));
  assert(g);
  _alltoall_init(g, inputs, size);
  return lva_to_gva(g);
}
