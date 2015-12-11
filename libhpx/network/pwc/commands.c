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
#include <libhpx/debug.h>
#include <libhpx/action.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

#include "commands.h"

static int _delete_parcel_handler(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("releasing sent parcel %p\n", (void*)p);
  parcel_delete(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(delete_parcel, _delete_parcel_handler);

static int _resume_parcel_remote_handler(int src, command_t command) {
  // bounce the command back to the src, because that is where the parcel to be
  // resumed is waiting
  return network_command(here->network, HPX_THERE(src), resume_parcel,
                         command.packed);
}
COMMAND_DEF(resume_parcel_remote, _resume_parcel_remote_handler);

static int _resume_parcel_handler(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("resuming %s, (%p)\n", actions[p->action].key,
          (void*)p);
  parcel_launch(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(resume_parcel, _resume_parcel_handler);

static HPX_USED const char *_straction(hpx_action_t id) {
  dbg_assert(here);
  CHECK_ACTION(id);
  return actions[id].key;
}

int command_run(int src, command_t cmd) {
  hpx_addr_t op = command_get_op(cmd);
  log_net("invoking command: %s from %d\n", _straction(op), src);
  CHECK_ACTION(op);
  command_handler_t f = (command_handler_t)(actions[op].handler);
  return f(src, cmd);
}
