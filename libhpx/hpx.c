// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

/// @file libhpx/hpx.c
/// @brief Implements much of hpx.h using libhpx.
///
/// This file implements the "glue" between the HPX public interface, and
/// libhpx.

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/percolation.h>
#include <libhpx/process.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/time.h>
#include <libhpx/topology.h>

#ifdef HAVE_APEX
# include "apex.h"
#endif

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated.
static void _cleanup(locality_t *l) {
  as_leave();

  if (l->tracer) {
    trace_destroy(l->tracer);
    l->tracer = NULL;
  }

  if (l->sched) {
    scheduler_delete(l->sched);
    l->sched = NULL;
  }

#ifdef HAVE_APEX
  apex_finalize();
#endif

  if (l->net) {
    network_delete(l->net);
    l->net = NULL;
  }

  if (l->percolation) {
    percolation_delete(l->percolation);
    l->percolation = NULL;
  }

  if (l->gas) {
    gas_dealloc(l->gas);
    l->gas = NULL;
  }

  dbg_fini();

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

  if (l->topology) {
    topology_delete(l->topology);
    l->topology = NULL;
  }

  spmd_fini();

  action_table_finalize();

  if (l->config) {
    config_delete(l->config);
  }

  free(l);
}

int hpx_init(int *argc, char ***argv) {
  int status = HPX_SUCCESS;

  // Start the internal clock
  libhpx_time_start();

  here = malloc(sizeof(*here));
  if (!here) {
    status = log_error("failed to allocate a locality.\n");
    goto unwind0;
  }

  here->rank = -1;
  here->ranks = 0;
  here->epoch = 0;

  sigset_t set;
  sigemptyset(&set);
  dbg_check(pthread_sigmask(SIG_BLOCK, &set, &here->mask));

  here->config = config_new(argc, argv);
  if (!here->config) {
    status = log_error("failed to create a configuration.\n");
    goto unwind1;
  }

  // check to see if everyone is waiting
  if (config_dbg_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
    dbg_wait();
  }

  // bootstrap
  here->boot = boot_new(here->config->boot);
  if (!here->boot) {
    status = log_error("failed to bootstrap.\n");
    goto unwind1;
  }
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // initialize the debugging system
  // @todo We would like to do this earlier but MPI_init() for the bootstrap
  //       network overwrites our segv handler.
  if (LIBHPX_OK != dbg_init(here->config)) {
    goto unwind1;
  }

  // Now that we know our rank, we can be more specific about waiting.
  if (config_dbg_waitat_isset(here->config, here->rank)) {
    // Don't wait twice.
    if (!config_dbg_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
      dbg_wait();
    }
  }

  // Initialize the tracing backend---have to wait until after bootstrap is
  // initialized because it checks to see if this rank is tracing.
  here->tracer = trace_new(here->config);

  // see if we're supposed to output the configuration, only do this at rank 0
  if (config_log_level_isset(here->config, HPX_LOG_CONFIG)) {
    if (here->rank == 0) {
      config_print(here->config, stdout);
    }
  }

  // initialize the spmd process functionality
  spmd_init();

  // topology discovery and initialization
  here->topology = topology_new(here->config);
  if (!here->topology) {
    status = log_error("failed to discover topology.\n");
    goto unwind1;
  }

  // Allocate the global heap.
  here->gas = gas_new(here->config, here->boot);
  if (!here->gas) {
    status = log_error("failed to create the global address space.\n");
    goto unwind1;
  }
  HPX_HERE = HPX_THERE(here->rank);

  here->percolation = percolation_new();
  if (!here->percolation) {
    status = log_error("failed to activate percolation.\n");
    goto unwind1;
  }

  int cores = system_get_available_cores();
  dbg_assert(cores > 0);

  if (!here->config->threads) {
    here->config->threads = cores;
  }
  log_dflt("HPX running %d worker threads on %d cores\n", here->config->threads,
           cores);

  here->net = network_new(here->config, here->boot, here->gas);
  if (!here->net) {
    status = log_error("failed to create network.\n");
    goto unwind1;
  }

#ifdef HAVE_APEX
  // initialize APEX, give this main thread a name
  apex_init("HPX WORKER THREAD");
  apex_set_node_id(here->rank);
#endif

  // thread scheduler
  here->sched = scheduler_new(here->config);
  if (!here->sched) {
    status = log_error("failed to create scheduler.\n");
    goto unwind1;
  }

  action_registration_finalize();
  trace_start(here->tracer);
  return status;
 unwind1:
  _cleanup(here);
 unwind0:
  return status;
}

static int
_run(int spmd, hpx_parcel_t *p)
{
  log_dflt("hpx started running %"PRIu64"\n", here->epoch);
  int status = scheduler_start(here->sched, p, spmd);
  log_dflt("hpx stopped running %"PRIu64"\n", here->epoch);
  here->epoch++;
  boot_barrier(here->boot);
  return status;
}

/// Called to run HPX.
int
_hpx_run(hpx_action_t *act, int n, ...)
{
  va_list args;
  va_start(args, n);
  int status = _run(0, action_new_parcel_va(*act, HPX_HERE, 0, 0, n, &args));
  va_end(args);
  return status;
}

int
_hpx_run_spmd(hpx_action_t *act, int n, ...)
{
  va_list args;
  va_start(args, n);
  hpx_parcel_t *p = action_new_parcel_va(*act, HPX_HERE, HPX_THERE(0),
                                         spmd_epoch_terminate, n, &args);
  int status = _run(1, p);
  va_end(args);
  return status;
}

int hpx_get_my_rank(void) {
  return (here) ? here->rank : -1;
}

int hpx_get_num_ranks(void) {
  return (here && here->boot) ? here->ranks : -1;
}

int hpx_get_num_threads(void) {
  return (here && here->sched) ? here->sched->n_workers : -1;
}

int hpx_is_active(void) {
  return (self->current != NULL);
}

/// Called by the application to terminate the scheduler and network.
void hpx_exit(int code) {
  dbg_assert_str(here->ranks,
                 "hpx_exit can only be called when the system is running.\n");

  uint64_t c = (uint32_t)code;

  // Loop through, sending the shutdown event to every locality. We use the
  // network_send operation manually here because it allows us to wait for the
  // `ssync` event (this event means that we're guaranteed that we don't need to
  // keep progressing locally for the send to be seen remotely).
  //
  // Don't perform the local shutdown until we're sure all the remote shutdowns
  // have gotten out, otherwise we might not progress the network enough.
  hpx_addr_t sync = hpx_lco_and_new(here->ranks - 1);
  for (int i = 0, e = here->ranks; i < e; ++i) {
    if (i != here->rank) {
      hpx_parcel_t *p = action_new_parcel(locality_stop, // action
                                          HPX_THERE(i),  // target
                                          0,             // continuation target
                                          0,             // continuation action
                                          1,             // number of args
                                          &c);           // the exit code
      hpx_parcel_t *q = action_new_parcel(hpx_lco_set_action, // action
                                          sync,          // target
                                          0,             // continuation target
                                          0,             // continuation action                                          0,
                                          0);            // number of args
      dbg_check( network_send(here->net, p, q) );
    }
  }
  dbg_check( hpx_lco_wait(sync) );
  hpx_lco_delete_sync(sync);

  // Call our own shutdown through cc, which orders it locally after the effects
  // from the loop above.
  int e = hpx_call_cc(HPX_HERE, locality_stop, &c);
  hpx_thread_exit(e);
}

/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(void) {
  if (here && here->config && here->config->dbg_waitonabort) {
    dbg_wait();
  }
  if (here && here->boot) {
    assert(here->boot);
    boot_abort(here->boot);
  }
  abort();
}

const char *hpx_strerror(hpx_status_t s) {
  switch (s) {
   case (HPX_ERROR): return "HPX_ERROR";
   case (HPX_SUCCESS): return "HPX_SUCCESS";
   case (HPX_RESEND): return "HPX_RESEND";
   case (HPX_LCO_ERROR): return "HPX_LCO_ERROR";
   case (HPX_LCO_CHAN_EMPTY): return "HPX_LCO_CHAN_EMPTY";
   case (HPX_LCO_TIMEOUT): return "HPX_LCO_TIMEOUT";
   case (HPX_LCO_RESET): return "HPX_LCO_RESET";
   case (HPX_USER): return "HPX_USER";
   default: return "HPX undefined error value";
  }
}

void hpx_finalize(void) {
  _cleanup(here);
}
