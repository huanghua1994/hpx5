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

#include <libhpx/debug.h>
#include "transport.h"

void *pwc_transport_new(const config_t *cfg, struct boot *boot) {
  switch (cfg->transport) {
   case (HPX_TRANSPORT_MPI):
    dbg_error("MPI support for the PWC network is not yet available.\n");

   case (HPX_TRANSPORT_PORTALS):
    dbg_error("Portals support for the PWC network is not yet available.\n");

   case (HPX_TRANSPORT_PHOTON):
#ifdef HAVE_PHOTON
    return pwc_transport_new_photon(cfg, boot);
#else
    dbg_error("Photon transport not enabled in current configuration.\n");
#endif

   default:
#ifdef HAVE_PHOTON
    return pwc_transport_new_photon(cfg, boot);
#else
    dbg_error("Photon transport required for PWC network.\n");
#endif
  }
  unreachable();
}

