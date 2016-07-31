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

/// The rendezvous send functionality is implemented as a request-get-reply
/// operation. The initial request operation is sent as a small parcel that we
/// know will not use rendezvous. That request operation will run as an
/// interrupt, and allocate a buffer to recv the large parcel being sent. It
/// will then initiate a get-with-completion operation that copies the parcel to
/// the local buffer. Attached to that get-with-completion operation are two
/// events, the remote event will free the sent parcel, while the local event
/// will schedule the parcel once the get has completed.

#include "pwc.h"
#include "commands.h"
#include "xport.h"
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

using namespace libhpx::network::pwc;

/// The local event handler for the get-with-completion operation.
///
/// This is used to schedule the transferred parcel once the get operation has
/// completed. The command encodes the local address of the parcel to schedule.
void
Command::rendezvousLaunch(unsigned src) const
{
  hpx_parcel_t *p = reinterpret_cast<hpx_parcel_t*>(arg_);
  parcel_set_state(p, PARCEL_SERIALIZED);
  EVENT_PARCEL_RECV(p->id, p->action, p->size, p->src, p->target);
  scheduler_spawn(p);
}

namespace {
struct _rendezvous_get_args_t {
  unsigned         rank;
  const hpx_parcel_t *p;
  size_t              n;
  xport_key_t       key;
};
}

/// The rendezvous request handler.
///
/// This handler will allocate a parcel to "get" into, and then initiate the
/// get-with-completion operation. It does not need to persist across the get
/// operation because it can attach the delete_parcel and _rendezvous_launch
/// event handlers to the get operation.
///
/// We need to use a marshaled operation because we send the transport key by
/// value and we don't have an FFI type to capture that.
static int
_rendezvous_get_handler(_rendezvous_get_args_t *args, size_t size)
{
  hpx_parcel_t *p = parcel_alloc(args->n - sizeof(*p));
  dbg_assert(p);
  xport_op_t op;
  op.rank = args->rank;
  op.n = args->n;
  op.dest = p;
  op.dest_key = pwc_network->xport->key_find_ref(pwc_network->xport, p, args->n);
  op.src = args->p;
  op.src_key = &args->key;
  op.lop = Command::RendezvousLaunch(p);
  op.rop = Command::DeleteParcel(args->p);
  int e = pwc_network->xport->gwc(&op);
  dbg_check(e, "could not issue get during rendezvous parcel\n");
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_MARSHALLED, _rendezvous_get,
                     _rendezvous_get_handler, HPX_POINTER, HPX_SIZE_T);

int
libhpx::network::pwc::pwc_rendezvous_send(void *network, const hpx_parcel_t *p)
{
  pwc_network_t *pwc = static_cast<pwc_network_t*>(network);
  size_t n = parcel_size(p);
  _rendezvous_get_args_t args = {
    .rank = here->rank,
    .p = p,
    .n = n
  };
  pwc->xport->key_find(pwc->xport, p, n, &args.key);
  return hpx_call(p->target, _rendezvous_get, HPX_NULL, &args, sizeof(args));
}