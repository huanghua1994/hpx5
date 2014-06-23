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
/// @file libhpx/scheduler/sema.c
/// @brief Implements the semaphore LCO.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

/// ----------------------------------------------------------------------------
/// Local semaphore interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t       lco;
  cvar_t    avail;
  uintptr_t count;
} _sema_t;


// Freelist
static __thread _sema_t *_free_semas = NULL;


static void
_free(_sema_t *sema)
{
  // repurpose count as a freelist "next" pointer
  sema->count = (uintptr_t)_free_semas;
  _free_semas = sema;
}


static void
_sema_fini(lco_t *lco)
{
  if (!lco)
    return;

  _sema_t *sema = (_sema_t *)lco;
  lco_lock(&sema->lco);
  _free(sema);
}


static void
_sema_error(lco_t *lco, hpx_status_t code)
{
  _sema_t *sema = (_sema_t *)lco;
  lco_lock(&sema->lco);
  scheduler_signal_error(&sema->avail, code);
  lco_unlock(&sema->lco);
}


/// Set is equivalent to returning a resource to the semaphore.
static void
_sema_set(lco_t *lco, int size, const void *from)
{
  _sema_t *sema = (_sema_t *)lco;
  lco_lock(&sema->lco);
  if (sema->count++ == 0) {
    // only signal one sleeping thread since we're only returning one resource,
    // waking everyone up is inefficient
    scheduler_signal(&sema->avail);
  }

  lco_unlock(&sema->lco);
}


static hpx_status_t
_sema_wait(lco_t *lco) {
  hpx_status_t status = HPX_SUCCESS;
  _sema_t *sema = (_sema_t *)lco;
  lco_lock(&sema->lco);

  // wait until the count is non-zero, use while here and re-read count because
  // our condition variables have MESA semantics
  unsigned count = sema->count;
  while (count == 0 && status == HPX_SUCCESS) {
    status = scheduler_wait(&sema->lco.lock, &sema->avail);
    count = sema->count;
  }

  // reduce the count, unless there was an error
  if (status == HPX_SUCCESS)
    sema->count = count - 1;

  lco_unlock(&sema->lco);
  return status;
}


static hpx_status_t
_sema_get(lco_t *lco, int size, void *out) {
  assert(size == 0);
  return _sema_wait(lco);
}


static void
_sema_init(_sema_t *sema, unsigned count)
{
  // the semaphore vtable
  static const lco_class_t vtable = {
    _sema_fini,
    _sema_error,
    _sema_set,
    _sema_get,
    _sema_wait
  };

  lco_init(&sema->lco, &vtable, 0);
  cvar_reset(&sema->avail);
  sema->count = count;
}

/// @}


/// ----------------------------------------------------------------------------
/// Allocate a semaphore LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_sema_new(unsigned count)
{
  hpx_addr_t sema;
  _sema_t *local = _free_semas;
  if (local) {
    _free_semas = (_sema_t *)local->count;
    sema = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(sema, (void**)&base)) {
      dbg_error("Could not translate local block.\n");
      hpx_abort();
    }
    hpx_gas_unpin(sema);
    sema.offset = (char*)local - base;
    assert(sema.offset < sema.block_bytes);
  }
  else {
    sema = hpx_gas_alloc(sizeof(_sema_t));
    if (!hpx_gas_try_pin(sema, (void**)&local)) {
      dbg_error("Could not pin newly allocated semaphore.\n");
      hpx_abort();
    }
  }
  _sema_init(local, count);
  hpx_gas_unpin(sema);
  return sema;
}


/// ----------------------------------------------------------------------------
/// Decrement a semaphore.
///
/// If the semaphore is local, then we can use the _sema_get operation directly,
/// otherwise we perform the operation as a synchronous remote call using the
/// _sema_p action.
///
/// @param sema - the global address of the semaphore we're reducing
/// @returns    - HPX_SUCCESS, or an error code if the sema is in an error state
/// ----------------------------------------------------------------------------
hpx_status_t
hpx_lco_sema_p(hpx_addr_t sema)
{
  return hpx_lco_get(sema, 0, NULL);
}


/// ----------------------------------------------------------------------------
/// Increment a semaphore.
///
/// If the semaphore is local, then we can use the _sema_set operation directly,
/// otherwise we perform the operation as an asynchronous remote call using the
/// _sema_v action.
///
/// @param sema - the global address of the semaphore we're incrementing
/// ----------------------------------------------------------------------------
void
hpx_lco_sema_v(hpx_addr_t sema)
{
  hpx_lco_set(sema, 0, NULL, HPX_NULL, HPX_NULL);
}
