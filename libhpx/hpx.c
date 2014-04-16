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

/// ----------------------------------------------------------------------------
/// @file libhpx/hpx.c
/// @brief Implements much of hpx.h using libhpx.
///
/// This file implements the "glue" between the HPX public interface, and
/// libhpx.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>

#include "hpx/hpx.h"
#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

#include "network/allocator.h"
#include "network/heavy.h"


/// ----------------------------------------------------------------------------
/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated, and return
/// the passed code.
/// ----------------------------------------------------------------------------
static int _cleanup(locality_t *l, int code) {
  if (l->sched) {
    scheduler_delete(l->sched);
    l->sched = NULL;
  }

  if (l->network) {
    network_delete(l->network);
    l->network = NULL;
  }

  if (l->transport) {
    transport_delete(l->transport);
    l->transport = NULL;
  }

  if (l->btt) {
    btt_delete(l->btt);
    l->btt = NULL;
  }

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

  return code;
}


static void *_map_local(uint32_t bytes) {
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;
  return mmap(NULL, bytes, prot, flags, -1, 0);
}


static hpx_action_t _bcast;


typedef struct {
  hpx_action_t action;
  size_t len;
  char *data[];
} _bcast_args_t;


static int _bcast_action(_bcast_args_t *args) {
  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  for (int i = 0, e = here->ranks; i < e; ++i)
    hpx_call(HPX_THERE(i), args->action, args->data, args->len, and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return HPX_SUCCESS;
}


static HPX_CONSTRUCTOR void _init_actions(void) {
  _bcast = HPX_REGISTER_ACTION(_bcast_action);
}


int hpx_init(const hpx_config_t *cfg) {
  // 1) start by initializing the entire local data segment
  here = _map_local(UINT32_MAX);
  if (!here)
    return dbg_error("failed to map the local data segment.\n");

  // for debugging
  here->rank = -1;
  here->ranks = -1;

  // 1a) set the local allocation sbrk
  sync_store(&here->local_sbrk, sizeof(*here), SYNC_RELEASE);


  // 2) bootstrap, to figure out some topology information
  here->boot = boot_new();
  if (here->boot == NULL)
    return _cleanup(here, dbg_error("failed to create boot manager.\n"));

  // 3) grab the rank and ranks, these are used all over the place so we expose
  //    them directly
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // 3a) set the global allocation sbrk
  sync_store(&here->global_sbrk, here->ranks, SYNC_RELEASE);

  // 4) update the HPX_HERE global address
  HPX_HERE = HPX_THERE(here->rank);

  // 5) allocate our block translation table
  here->btt = btt_new(cfg->gas);
  if (here->btt == NULL)
    return _cleanup(here, dbg_error("failed to create the block translation table.\n"));

  // 6) allocate the transport
  here->transport = transport_new(cfg->transport);
  if (here->transport == NULL)
    return _cleanup(here, dbg_error("failed to create transport.\n"));
  dbg_log("initialized the %s transport.\n", transport_id(here->transport));

  here->network = network_new();
  if (here->network == NULL)
    return _cleanup(here, dbg_error("failed to create network.\n"));
  dbg_log("initialized the network.\n");

  // 7) insert the base mapping for our local data segment, and pin it so that
  //    it doesn't go anywhere, ever....
  btt_insert(here->btt, HPX_HERE, here);
  void *local;
  bool pinned = hpx_addr_try_pin(HPX_HERE, &local);
  assert(local == here);
  assert(pinned);

  int cores = (cfg->cores) ? cfg->cores : system_get_cores();
  int workers = (cfg->threads) ? cfg->threads : cores;
  int stack_size = cfg->stack_bytes;
  here->sched = scheduler_new(cores, workers, stack_size);
  if (here->sched == NULL)
    return _cleanup(here, dbg_error("failed to create scheduler.\n"));

  return HPX_SUCCESS;
}


/// Called to run HPX.
int hpx_run(hpx_action_t act, const void *args, unsigned size) {
  // start the network
  pthread_t heavy;
  int e = pthread_create(&heavy, NULL, heavy_network, here->network);
  if (e)
    return _cleanup(here, dbg_error("could not start the network thread.\n"));

  // the rank-0 process starts the application by sending a single parcel to
  // itself
  if (here->rank == 0) {
    // allocate and initialize a parcel for the original action
    hpx_parcel_t *p = hpx_parcel_acquire(size);
    if (!p)
      return dbg_error("failed to allocate an initial parcel.\n");

    p->action = act;
    hpx_parcel_set_data(p, args, size);

    // Don't use hpx_parcel_send() here, because that will try and enqueue the
    // parcel locally, but we're not a scheduler thread yet, and haven't
    // initialized the appropriate structures. Network loopback will get this to
    // us once we start progressing and receive from the network.
    network_send(here->network, p);
  }

  // start the scheduler, this will return after scheduler_shutdown()
  e = scheduler_startup(here->sched);

  // wait for the network to shutdown
  e = pthread_join(heavy, NULL);
  if (e) {
    dbg_error("could not join the heavy network thread.\n");
    return e;
  }

  // and cleanup the system
  return _cleanup(here, e);
}

/// Encapsulates a remote-procedure-call.
int hpx_call(hpx_addr_t target, hpx_action_t action, const void *args,
             size_t len, hpx_addr_t result) {
  hpx_parcel_t *p = hpx_parcel_acquire(len);
  if (!p) {
    dbg_error("could not allocate parcel.\n");
    return 1;
  }

  p->action = action;
  p->target = target;
  p->cont = result;
  hpx_parcel_set_data(p, args, len);
  hpx_parcel_send(p);
  return HPX_SUCCESS;
}


/// Encapsulates a RPC called on all available localities.
int hpx_bcast(hpx_action_t action, const void *data, size_t len, hpx_addr_t lco) {
  hpx_parcel_t *p = hpx_parcel_acquire(len + sizeof(_bcast_args_t));
  p->target = HPX_HERE;
  p->action = _bcast;
  p->cont = lco;

  _bcast_args_t *args = (_bcast_args_t *)p->data;
  args->action = action;
  args->len = len;
  memcpy(&args->data, data, len);
  hpx_parcel_send(p);

  return HPX_SUCCESS;
}


hpx_action_t hpx_register_action(const char *id, hpx_action_handler_t func) {
  return action_register(id, func);
}


void hpx_parcel_send(hpx_parcel_t *p) {
  if (hpx_addr_try_pin(p->target, NULL))
    scheduler_spawn(p);
  else
    network_send(here->network, p);
}


hpx_parcel_t *hpx_parcel_acquire(size_t size) {
  // get a parcel of the right size from the allocator, the returned parcel
  // already has its data pointer and size set appropriately
  hpx_parcel_t *p = parcel_allocator_get(size);
  if (!p) {
    dbg_error("failed to get an %lu-byte parcel from the allocator.\n", size);
    return NULL;
  }

  p->pid    = -1;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_HERE;
  p->cont   = HPX_NULL;
  return p;
}


void hpx_parcel_release(hpx_parcel_t *p) {
  parcel_allocator_put(p);
}


int hpx_get_my_rank(void) {
  assert(here);
  return here->rank;
}


int hpx_get_num_ranks(void) {
  assert(here);
  return here->ranks;
}


int hpx_get_num_threads(void) {
  if (!here || !here->sched)
    return 0;
  return here->sched->n_workers;
}


const char *hpx_get_network_id(void) {
  if (!here || !here->transport)
    return "cannot query network now";
  return transport_id(here->transport);
}

void system_shutdown(int code) {
  if (!here || !here->sched) {
    dbg_error("hpx_shutdown called without a scheduler.\n");
    abort();
  }

  scheduler_shutdown(here->sched);
}


/// Called by the application to terminate the scheduler and network.
void hpx_shutdown(int code) {
  // do an asynchronous broadcast of shutdown requests
  hpx_bcast(locality_shutdown, NULL, 0, HPX_NULL);
  hpx_thread_exit(HPX_SUCCESS);
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(void) {
  abort();
}


/// ----------------------------------------------------------------------------
/// This is currently trying to provide the layout:
///
/// shared [1] T foo[n]; where sizeof(T) == bytes
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_global_alloc(size_t n, uint32_t bytes) {
  // Get a set of @p n contiguous block ids.
  uint32_t base_id;
  hpx_addr_t f = hpx_lco_future_new(sizeof(base_id));
  hpx_call(HPX_THERE(0), locality_global_sbrk, &n, sizeof(n), f);
  hpx_lco_get(f, &base_id, sizeof(base_id));
  hpx_lco_delete(f, HPX_NULL);

  int ranks = here->ranks;
  uint32_t blocks_per_locality = n / ranks + ((n % ranks) ? 1 : 0);
  uint32_t args[3] = {
    base_id,
    blocks_per_locality,
    bytes
  };

  // The global alloc is currently synchronous, because the btt mappings aren't
  // complete until the allocation is complete.
  hpx_addr_t and = hpx_lco_and_new(ranks);
  for (int i = 0; i < ranks; ++i)
    hpx_call(HPX_THERE(i), locality_alloc_blocks, &args, sizeof(args), and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // Return the base id to the caller.
  return hpx_addr_init(0, base_id, bytes);
}


bool hpx_addr_try_pin(const hpx_addr_t addr, void **local) {
  return btt_try_pin(here->btt, addr, local);
}


void hpx_addr_unpin(const hpx_addr_t addr) {
  btt_unpin(here->btt, addr);
}


/// ----------------------------------------------------------------------------
/// Local allocation is done from our designated local block. Allocation is
/// always done to 8 byte alignment. Here we're using a simple sbrk allocator
/// with no free functionality for now.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_alloc(size_t bytes) {
  bytes += bytes % 8;
  uint32_t offset = sync_fadd(&here->local_sbrk, bytes, SYNC_ACQ_REL);
  if (UINT32_MAX - offset < bytes) {
    dbg_error("exhausted local allocation limit with %lu-byte allocation.\n",
              bytes);
    hpx_abort();
  }

  return hpx_addr_add(HPX_HERE, offset);
}


void hpx_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco) {
  hpx_gas_t type = btt_type(here->btt);
  if ((type == HPX_GAS_PGAS) || (type == HPX_GAS_PGAS_SWITCH)) {
    if (!hpx_addr_eq(lco, HPX_NULL))
      hpx_lco_set(lco, NULL, 0, HPX_NULL);
    return;
  }

  hpx_call(dst, locality_move_block, &src, sizeof(src), lco);
}
