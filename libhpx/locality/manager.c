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
#include <stddef.h>
#include "attributes.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/locality/manager.c
/// @brief "placeholder" weak implementations to prevent linker complaints
/// ----------------------------------------------------------------------------

HPX_INTERNAL void *manager_new_mpirun(void) HPX_WEAK;
HPX_INTERNAL void *manager_new_pmi(void) HPX_WEAK;
HPX_INTERNAL void *manager_new_smp(void) HPX_WEAK;

void *
manager_new_mpirun(void) {
  return NULL;
}

void *
manager_new_pmi(void) {
  return NULL;
}

void *
manager_new_smp(void) {
  return NULL;
}
