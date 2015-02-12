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

/// @file libhpx/hpx.c
/// @brief Implements much of hpx.h using libhpx.
///
/// This file implements the "glue" between the HPX public interface, and
/// libhpx.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/instrumentation.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
#include "network/probe.h"

static HPX_ACTION(_hpx_143_fix, void *UNUSED) {
  hpx_gas_global_alloc(sizeof(void*), HPX_LOCALITIES);
  return LIBHPX_OK;
}

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated.
static void _cleanup(locality_t *l) {
  if (!l)
    return;

  if (l->sched) {
    scheduler_delete(l->sched);
    l->sched = NULL;
  }

  if (l->network) {
    network_delete(l->network);
    l->network = NULL;
  }

  if (l->gas) {
    gas_delete(l->gas);
    l->gas = NULL;
  }

  if (l->transport) {
    transport_delete(l->transport);
    l->transport = NULL;
  }

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

  hwloc_topology_destroy(l->topology);

  if (l->actions) {
    action_table_free(l->actions);
  }

  inst_fini();

  if (l->config) {
    config_delete(l->config);
  }

  free(l);
}


int hpx_init(int *argc, char ***argv) {
  int status = HPX_SUCCESS;

  here = malloc(sizeof(*here));
  if (!here) {
    status = log_error("failed to allocate a locality.\n");
    goto unwind0;
  }

  here->rank = -1;
  here->ranks = 0;
  here->actions = NULL;

  here->config = config_new(argc, argv);
  if (!here->config) {
    status = log_error("failed to create a configuration.\n");
    goto unwind1;
  }

  // check to see if everyone is waiting
  if (config_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
    dbg_wait();
  }

  // set the log level
  if (config_logat_isset(here->config, HPX_LOCALITY_ALL)) {
    log_level = here->config->loglevel;
  }

  // topology
  int e = hwloc_topology_init(&here->topology);
  if (e) {
    status = log_error("failed to initialize a topology.\n");
    goto unwind1;
  }
  e = hwloc_topology_load(here->topology);
  if (e) {
    status = log_error("failed to load the topology.\n");
    goto unwind1;
  }

  // bootstrap
  here->boot = boot_new(here->config->boot);
  if (!here->boot) {
    status = log_error("failed to bootstrap.\n");
    goto unwind1;
  }
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // Now that we know our rank, we can be more specific about waiting.
  if (config_waitat_isset(here->config, here->rank)) {
    // Don't wait twice.
    if (!config_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
      dbg_wait();
    }
  }

  if (inst_init(here->config)) {
    log("error detected while initializing instrumentation\n");
  }

  // Reset the log level based on our rank-specific information.
  if (config_logat_isset(here->config, here->rank)) {
    log_level = here->config->loglevel;
  }

  // byte transport
  here->transport = transport_new(here->config->transport,
                                  here->config->sendlimit,
                                  here->config->recvlimit);
  if (!here->transport) {
    status = log_error("failed to create transport.\n");
    goto unwind1;
  }

  // global address space
  here->gas = gas_new(here->config->heapsize, here->boot, here->transport,
                      here->config->gas);
  if (!here->gas) {
    status = log_error("failed to create the global address space.\n");
    goto unwind1;
  }
  if (here->gas->join()) {
    status = log_error("failed to join the global address space.\n");
    goto unwind1;
  }
  HPX_HERE = HPX_THERE(here->rank);

  if (!here->config->cores) {
    here->config->cores = system_get_cores();
  }

  if (!here->config->threads) {
    here->config->threads = here->config->cores;
  }

  // parcel network
  here->network = network_new(here->config, here->boot, here->gas,
                              here->config->threads);
  if (!here->network) {
    status = log_error("failed to create network.\n");
    goto unwind1;
  }

  // thread scheduler
  here->sched = scheduler_new(here->config);
  if (!here->sched) {
    status = log_error("failed to create scheduler.\n");
    goto unwind1;
  }

  return status;
 unwind1:
  _cleanup(here);
 unwind0:
  return status;
}


/// Called to run HPX.
int _hpx_run(hpx_action_t *act, int nargs, ...) {
  int status = HPX_SUCCESS;
  if (!here || !here->sched) {
    status = log_error("hpx_init() must be called before hpx_run()\n");
    goto unwind0;
  }

  here->actions = action_table_finalize();
  if (!here->actions) {
    status = log_error("failed to finalize the action table.\n");
    goto unwind0;
  }

  // pthread_t _progress = progress_start(here);
  // if (pthread_equal(_progress, pthread_self())) {
  //   dbg_error("failed to start progress\n");
  // }

  if (probe_start(here->network) != LIBHPX_OK) {
    status = log_error("could not start network probe\n");
    goto unwind1;
  }

  // create the initial application-level thread to run
  if (here->rank == 0) {
    va_list vargs;
    va_start(vargs, nargs);
    status = libhpx_call_action(here->actions, HPX_HERE, *act, HPX_NULL,
                                HPX_ACTION_NULL, HPX_NULL, HPX_NULL,
                                nargs, &vargs);
    va_end(vargs);
    if (status != LIBHPX_OK) {
      log_error("failed to spawn initial action\n");
      goto unwind2;
    }

    // Fix for https://uisapp2.iu.edu/jira-prd/browse/HPX-143
    status = hpx_call(HPX_HERE, _hpx_143_fix, HPX_NULL, NULL, 0);
    if (status != LIBHPX_OK) {
      log_error("failed to spawn the initial cyclic allocation");
      goto unwind2;
    }
  }

  // start the scheduler, this will return after scheduler_shutdown()
  if (scheduler_startup(here->sched) != LIBHPX_OK) {
    log_error("scheduler shut down with error.\n");
    goto unwind2;
  }

#ifdef ENABLE_PROFILING
  scheduler_dump_stats(here->sched);
#endif

 unwind2:
  // progress_stop(_progress);
  probe_stop();
 unwind1:
  _cleanup(here);
 unwind0:
  return status;
}


int hpx_get_my_rank(void) {
  return (here) ? here->rank : -1;
}


int hpx_get_num_ranks(void) {
  return (here && here->boot) ? here->ranks : -1;
}


int hpx_get_num_threads(void) {
  return (here && here->sched) ? here->sched->n_workers : 0;
}


/// Called by the application to terminate the scheduler and network.
void hpx_shutdown(int code) {
  dbg_assert_str(here->ranks,
                 "hpx_shutdown can only be called when the system is running.\n");

  // make sure we flush our local network when we shutdown
  network_flush_on_shutdown(here->network);
  int e = hpx_bcast(locality_shutdown, HPX_NULL, &code, sizeof(code));
  hpx_thread_exit(e);
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(void) {
  inst_fini();

  if (here && here->config && here->config->waitonabort) {
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
