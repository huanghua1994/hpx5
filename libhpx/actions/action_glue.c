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

#include <libhpx/action.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "table.h"

bool action_is_pinned(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_pinned(entry);
}

bool action_is_marshalled(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_marshalled(entry);
}

bool action_is_vectored(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_vectored(entry);
}

bool action_is_internal(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_internal(entry);
}

bool action_is_default(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_default(entry);
}

bool action_is_task(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_task(entry);
}

bool action_is_interrupt(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_interrupt(entry);
}

bool action_is_function(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_function(entry);
}

bool action_is_opencl(const action_entry_t *table, hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_opencl(entry);
}

hpx_action_handler_t hpx_action_get_handler(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return (hpx_action_handler_t)entry->handler;
}

int action_call_va(hpx_addr_t addr, hpx_action_t action, hpx_addr_t c_addr,
                   hpx_action_t c_action, hpx_addr_t lsync, hpx_addr_t gate,
                   int nargs, va_list *args) {
  hpx_parcel_t *p = action_create_parcel_va(addr, action, c_addr, c_action,
                                            nargs, args);

  if (likely(!gate && !lsync)) {
    parcel_launch(p);
    return HPX_SUCCESS;
  }
  if (!gate && lsync) {
    return hpx_parcel_send(p, lsync);
  }
  if (!lsync) {
    return hpx_parcel_send_through_sync(p, gate);
  }
  return hpx_parcel_send_through(p, gate, lsync);
}
