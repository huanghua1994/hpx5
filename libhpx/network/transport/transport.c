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
/// @file libhpx/network/transport/transport.c
/// @brief Handles transport initialization.
/// ----------------------------------------------------------------------------

#include "transport.h"
#include "debug.h"

transport_t *
transport_new(void) {
  transport_t *transport = NULL;

#ifdef HAVE_PHOTON
  transport = transport_new_photon();
  if (transport) {
    dbg_log("initialized the Photon transport.\n");
    return transport;
  }
#endif

#ifdef HAVE_MPI
  transport = transport_new_mpi();
  if (transport) {
    dbg_log("initialized the MPI transport.\n");
    return transport;
  }
#endif

  transport = transport_new_smp();
  if (transport) {
    dbg_log("initialized the SMP transport.\n");
    return transport;
  }

  dbg_error("failed to initialize a transport.\n");
  return NULL;
}
