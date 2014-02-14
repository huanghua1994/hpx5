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
/// @file action.c
///
/// Implements action registration and lookup.
/// ----------------------------------------------------------------------------

#include "action.h"

hpx_action_handler_t
action_for_key(hpx_action_t key) {
  return (hpx_action_handler_t)key;
}

hpx_action_t
hpx_action_register(const char *id, hpx_action_handler_t func) {
  return (hpx_action_t)func;
}
