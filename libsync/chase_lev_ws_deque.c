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
#ifdef CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// A workstealing deque implementation based on the design presented in
/// "Dynamic Circular Work-Stealing Deque" by David Chase and Yossi Lev
/// @url http://dl.acm.org/citation.cfm?id=1073974.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>

#include "libsync/deques.h"

/// ----------------------------------------------------------------------------
/// A Chase-Lev WS Deque buffer.
///
/// The buffer contains its capacity, and an inline block for the buffer
/// itself. Buffer's can't be resized themselves, they can only be replaced with
/// larger or smaller buffers.
/// ----------------------------------------------------------------------------
typedef struct chase_lev_ws_deque_buffer {
  struct chase_lev_ws_deque_buffer *parent;
  size_t capacity;
  void * buffer[];
} _buffer_t;


/// ----------------------------------------------------------------------------
/// Allocate a new buffer of the right capacity.
/// ----------------------------------------------------------------------------
static _buffer_t *_buffer_new(_buffer_t *parent, size_t capacity) {
  assert(capacity > 0);
  _buffer_t *b = malloc(sizeof(_buffer_t) + capacity * sizeof(void*));
  assert(b);
  b->parent = parent;
  b->capacity = capacity;
  return b;
}


/// ----------------------------------------------------------------------------
/// Delete a buffer, and all of its parents.
/// ----------------------------------------------------------------------------
static void _buffer_delete(_buffer_t *b) {
  if (!b)
    return;
  _buffer_t *parent = b->parent;
  free(b);
  _buffer_delete(parent);
}


/// ----------------------------------------------------------------------------
/// Insert into the buffer, modulo its capacity.
/// ----------------------------------------------------------------------------
static void _buffer_put(_buffer_t *b, uint64_t i, void *val) {
  b->buffer[i % b->capacity] = val;
}


/// ----------------------------------------------------------------------------
/// Lookup in the buffer, modulo its capacity.
/// ----------------------------------------------------------------------------
static void *_buffer_get(_buffer_t *b, uint64_t i) {
  return b->buffer[i % b->capacity];
}


/// ----------------------------------------------------------------------------
/// Grow a Chase-Lev WS Deque buffer.
///
/// @param    old - the current buffer
/// @param bottom - the deque bottom index
/// @param    top - the deque top index
/// @returns      - the new buffer
/// ----------------------------------------------------------------------------
static _buffer_t *_buffer_grow(_buffer_t *old, uint64_t bottom, uint64_t top) {
  _buffer_t *new = _buffer_new(old, 2 * old->capacity);
  for (; top < bottom; ++top)
    _buffer_put(new, top, _buffer_get(old, top));
  return new;
}


/// ----------------------------------------------------------------------------
/// Utility functions to set and CAS deque fields.
///
/// We don't have _get versions because we use different memory consistency
/// models for these in different places. Sets and CASes are always releases
/// though, based on how they are used.
/// ----------------------------------------------------------------------------
/// @{
static void _deque_set_bottom(chase_lev_ws_deque_t *deque, uint64_t val) {
  sync_store(&deque->bottom, val, SYNC_RELEASE);
}


static void _deque_set_buffer(chase_lev_ws_deque_t *deque, _buffer_t *buffer) {
  sync_store(&deque->buffer, buffer, SYNC_RELEASE);
}


static void _deque_set_top(chase_lev_ws_deque_t *deque, uint64_t top) {
  sync_store(&deque->top, top, SYNC_RELEASE);
}


static bool _deque_try_inc_top(chase_lev_ws_deque_t *deque, uint64_t top) {
  return sync_cas(&deque->top, top, top + 1, SYNC_RELEASE, SYNC_RELAXED);
}
/// @}


chase_lev_ws_deque_t *sync_chase_lev_ws_deque_new(size_t size) {
  chase_lev_ws_deque_t *deque = malloc(sizeof(*deque));
  if (deque)
    sync_chase_lev_ws_deque_init(deque, size);
  return deque;
}


void sync_chase_lev_ws_deque_init(chase_lev_ws_deque_t *d, size_t capacity) {
  // initialize vtable
  d->vtable.delete = (__typeof__(d->vtable.delete))sync_chase_lev_ws_deque_delete;
  d->vtable.push = (__typeof__(d->vtable.push))sync_chase_lev_ws_deque_push;
  d->vtable.pop = (__typeof__(d->vtable.pop))sync_chase_lev_ws_deque_pop;
  d->vtable.steal = (__typeof__(d->vtable.steal))sync_chase_lev_ws_deque_steal;

  _buffer_t *buffer = _buffer_new(NULL, capacity);
  assert(buffer);

  _deque_set_bottom(d, 1);
  _deque_set_top(d, 1);
  _deque_set_buffer(d, buffer);

  d->top_bound = 1;
}


void sync_chase_lev_ws_deque_fini(chase_lev_ws_deque_t *d) {
  _buffer_t *buffer = sync_load(&d->buffer, SYNC_RELAXED);
  if (buffer)
    _buffer_delete(buffer);
}


void sync_chase_lev_ws_deque_delete(chase_lev_ws_deque_t *d) {
  if (!d)
    return;
  sync_chase_lev_ws_deque_fini(d);
  free(d);
}

void sync_chase_lev_ws_deque_push(chase_lev_ws_deque_t *d, void *val) {
  // read bottom and top, and capacity
  uint64_t bottom = sync_load(&d->bottom, SYNC_RELAXED);
  _buffer_t *buffer = sync_load(&d->buffer, SYNC_RELAXED);
  uint64_t top = d->top_bound;                  // Chase-Lev 2.3

  // if the deque seems to be full then update its top bound
  if (bottom - top + 1 >= buffer->capacity) {
    top = d->top_bound = sync_load(&d->top, SYNC_ACQUIRE);
    // if the deque is *really* full then expand its capacity
    if (bottom - top + 1 >= buffer->capacity) {
      buffer = _buffer_grow(buffer, bottom, top);
      _deque_set_buffer(d, buffer);
    }
  }

  // update the bottom
  _buffer_put(buffer, bottom, val);
  _deque_set_bottom(d, bottom + 1);
}


void *sync_chase_lev_ws_deque_pop(chase_lev_ws_deque_t *d) {
  // read and update bottom
  uint64_t bottom = sync_load(&d->bottom, SYNC_RELAXED);
  bottom = bottom - 1;
  _deque_set_bottom(d, bottom);

  // read top
  uint64_t top = sync_load(&d->top, SYNC_ACQUIRE);
  int64_t size = bottom - top;

  // if the queue was empty, reset bottom
  if (size < 0) {
    _deque_set_bottom(d, top);
    return NULL;
  }

  // if the queue becomes empty, then try and race with a steal()er who might be
  // taking our last element, by updating top
  if (size == 0) {
    // if we win this race, then we need to update bottom to tpo, or it lags
    // behind release to steal()
    if (!_deque_try_inc_top(d, top))
      return NULL;
    _deque_set_bottom(d, bottom + 1);
  }

  // otherwise we successfully popped from the deque, just read the buffer and
  // return the value at the bottom
  _buffer_t *buffer = sync_load(&d->buffer, SYNC_RELAXED);
  return _buffer_get(buffer, bottom);
}


void *sync_chase_lev_ws_deque_steal(chase_lev_ws_deque_t *d) {
  while (true) {
    // read top and bottom
    // acquire from push()/pop()
    uint64_t top = sync_load(&d->top, SYNC_ACQUIRE);
    uint64_t bottom = sync_load(&d->bottom, SYNC_ACQUIRE);

    // if the deque seems to be empty, fail the steal
    if (bottom - top <= 0)
      return NULL;

    // read the buffer and the value, have to read the value before the CAS,
    // otherwise we could miss some push-pops and get the wrong value due to the
    // underlying cyclic array (see Chase-Lev 2.2)
    //
    // NB: it doesn't matter if the buffer grows a number of times between these
    //     two operations, because _buffer_get(top) will always return the same
    //     value---this is a result of the magic and beauty of this
    //     algorithm. If we want to shrink the buffer then we'll have to pay
    //     more attention.
    _buffer_t *buffer = sync_load(&d->buffer, SYNC_ACQUIRE);
    void *val = _buffer_get(buffer, top);

    // if we update the bottom, return the stolen value, otherwise retry
    // release to push()/pop()
    if (_deque_try_inc_top(d, top))
      return val;
  }
}
