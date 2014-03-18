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
#define _GNU_SOURCE /* pthread_setaffinity_np */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/worker.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include <sync/sync.h>
#include <sync/barriers.h>
#include <hpx.h>

#include "libhpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "thread.h"
#include "worker.h"


typedef SYNC_ATOMIC(int) atomic_int_t;
typedef SYNC_ATOMIC(atomic_int_t*) atomic_int_atomic_ptr_t;


/// ----------------------------------------------------------------------------
/// Class representing a worker thread's state.
///
/// Worker threads are "object-oriented" insofar as that goes, but each native
/// thread has exactly one, thread-local worker_t structure, so the interface
/// doesn't take a "this" pointer and instead grabs the "self" structure using
/// __thread local storage.
/// ----------------------------------------------------------------------------
/// @{
static __thread struct worker {
  pthread_t       thread;                       // this worker's native thread
  int                 id;                       // this workers's id
  int            core_id;                       // useful for "smart" stealing
  void               *sp;                       // this worker's native stack
  thread_t         *free;                       // local thread freelist
  thread_t        *ready;                       // local active threads
  thread_t         *next;                       // local active threads
  atomic_int_t  shutdown;                       // cooperative shutdown flag
  scheduler_t *scheduler;                       // the scheduler we belong to
} self = {
  .thread    = 0,
  .id        = -1,
  .core_id   = -1,
  .sp        = NULL,
  .free      = NULL,
  .ready     = NULL,
  .next      = NULL,
  .shutdown  = 0,
  .scheduler = NULL
};


/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs after a worker first starts it's
/// scheduling loop, but before any user defined lightweight threads run.
/// ----------------------------------------------------------------------------
static int _on_start(void *sp, void *env) {
  assert(sp);
  assert(self.scheduler);

  // checkpoint my native stack pointer
  self.sp = sp;

  // wait for the rest of the scheduler to catch up to me
  scheduler_barrier(self.scheduler, self.id);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Create a new lightweight thread based on the parcel.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be.
///
/// @param p - the parcel that is generating this thread.
/// @returns - a new lightweight thread, as defined by the parcel
/// ----------------------------------------------------------------------------
static thread_t *_bind(hpx_parcel_t *p) {
  thread_t *thread = thread_pop(&self.free);
  return (thread) ? thread_init(thread, p) : thread_new(p);
}


/// ----------------------------------------------------------------------------
/// Steal a lightweight thread during scheduling.
/// ----------------------------------------------------------------------------
static thread_t *_steal(void) {
  return NULL;
}


/// ----------------------------------------------------------------------------
/// Check the network during scheduling.
/// ----------------------------------------------------------------------------
static thread_t *_network(void) {
  hpx_parcel_t *p = scheduler_network_recv(self.scheduler);
  return (p) ? _bind(p) : NULL;
}


/// ----------------------------------------------------------------------------
/// The main scheduling "loop."
///
/// Selects a new lightweight thread to run. If @p fast is set then the
/// algorithm assumes that the calling thread (also a lightweight thread) really
/// wants to transfer quickly---likely because it is holding an LCO's lock and
/// would like to release it.
///
/// Scheduling quickly likely means not trying to perform a steal operation, and
/// not performing any standard maintenance tasks.
///
/// If @p final is non-null, then this indicates that the current thread, which
/// is a lightweight thread, is available for rescheduling, so the algorithm
/// takes that into mind. If the scheduling loop would like to select @p final,
/// but it is NULL, then the scheduler will return a new thread running the
/// HPX_ACTION_NULL action.
///
/// @param  fast - schedule quickly
/// @param final - a final option if the scheduler wants to give up
/// @returns     - a thread to transfer to
/// ----------------------------------------------------------------------------
static thread_t *_schedule(bool fast, thread_t *final) {
  // if we're supposed to shutdown, then do so
  if (sync_load(&self.shutdown, SYNC_ACQUIRE))
    thread_transfer(self.sp, &self.next, thread_checkpoint_push);

  // if there are ready threads, select the next one
  thread_t *t = thread_pop(&self.ready);
  if (t)
    return t;

  // no ready threads, perform an internal epoch transition
  self.ready = self.next;
  self.next = NULL;

  // try to get some work from the network, if we're not in a hurry
  if (!fast)
    if ((t = _network()))
      return t;

  // if the epoch switch has given us some work to do, go do it
  if (self.ready)
    return _schedule(fast, final);

  // try to steal some work, if we're not in a hurry
  if (!fast)
    if ((t = _steal()))
      return t;

  // as a last resort, return final, or a new empty action
  return (final) ? (final) : _bind(hpx_parcel_acquire(0));
}


/// ----------------------------------------------------------------------------
/// Run a worker thread.
///
/// This is the pthread entry function for a scheduler worker thread. It needs
/// to initialize any thread-local data, and then start up the scheduler. We do
/// this by creating an initial user-level thread and transferring to it.
///
/// Under normal HPX shutdown, we return to the original transfer site and
/// cleanup.
/// ----------------------------------------------------------------------------
typedef struct {
  int id;
  int core_id;
  scheduler_t *scheduler;
  sr_barrier_t *barrier;
  worker_t **worker;
} _run_args_t;

void *_run(void *run_args) {
  _run_args_t *args = run_args;

  // output
  *args->worker  = &self;

  self.thread    = pthread_self();
  self.id        = args->id;
  self.core_id   = args->core_id;
  self.scheduler = args->scheduler;

  // don't need my arguments anymore
  free(args);

  // set this thread's affinity
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(args->core_id, &cpuset);
  int e = pthread_setaffinity_np(self.thread, sizeof(cpuset), &cpuset);
  if (e) {
    dbg_error("failed to bind thread affinity for %d", self.id);
    return NULL;
  }

  // make myself asynchronously cancellable
  e = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  if (e) {
    dbg_error("failed to become async cancellable.\n");
    return NULL;
  }

  // I'm a real thread that can be canceled at this point, join the
  // synchronization barrier to let the parent thread return.
  sr_barrier_join(args->barrier, 0);

  // get a parcel to start the scheduler loop with
  hpx_parcel_t *p = hpx_parcel_acquire(0);
  if (!p) {
    dbg_error("failed to acquire an initial parcel.\n");
    return NULL;
  }

  // get a thread to transfer to
  thread_t *t = _bind(p);
  if (!t) {
    dbg_error("failed to bind an initial thread.\n");
    hpx_parcel_release(p);
    return NULL;
  }

  // transfer to the thread---ordinary shutdown will return here
  e = thread_transfer(t->sp, NULL, _on_start);
  if (e) {
    dbg_error("shutdown returned error\n");
    return NULL;
  }

  // cleanup the thread's resources---we only return here under normal shutdown
  // termination, otherwise we're canceled and vanish
  while (self.ready) {
    thread_t *t = self.ready;
    self.ready = self.ready->next;
    hpx_parcel_release(t->parcel);
    thread_delete(t);
  }

  while (self.next) {
    thread_t *t = self.next;
    self.next = self.next->next;
    hpx_parcel_release(t->parcel);
    thread_delete(t);
  }

  while (self.free) {
    thread_t *t = self.free;
    self.free = self.free->next;
    thread_delete(t);
  }

  return NULL;
}

worker_t *worker_start(int id, int core_id, scheduler_t *scheduler) {
  sr_barrier_t *barrier = sr_barrier_new(2);
  if (!barrier) {
    dbg_error("could not allocate a barrier in worker_start.\n");
    goto unwind0;
  }

  _run_args_t *args = malloc(sizeof(*args));
  if (!args) {
    dbg_error("could not allocate arguments for pthread entry, _run.\n");
    goto unwind1;
  }

  worker_t *out   = NULL;
  args->id        = id;
  args->core_id   = core_id;
  args->scheduler = scheduler;
  args->barrier   = barrier;
  args->worker    = &out;

  pthread_t thread;
  int e = pthread_create(&thread, NULL, _run, args);
  if (e) {
    dbg_error("failed to create worker thread #%d.\n", self.id);
    goto unwind2;
  }

  sr_barrier_join(barrier, 1);
  return out;

 unwind2:
  free(args);
 unwind1:
  sr_barrier_delete(barrier);
 unwind0:
  return NULL;
}


void worker_shutdown(worker_t *worker) {
  sync_store(&worker->shutdown, 1, SYNC_RELEASE);
  if (pthread_join(worker->thread, NULL))
    dbg_error("cannot join worker thread %d.\n", worker->id);
}


void worker_cancel(worker_t *worker) {
  if (!worker)
    return;

  if (pthread_cancel(worker->thread)) {
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
  }
  else if (pthread_join(worker->thread, NULL)) {
    dbg_error("cannot join worker thread %d.\n", worker->id);
  }
}


/// Spawn a user-level thread.
void scheduler_spawn(hpx_parcel_t *p) {
  assert(p);
  assert(hpx_addr_try_pin(hpx_parcel_get_target(p), NULL));
  abort();
}


/// Yields the current thread.
///
/// This doesn't block the current thread, but gives the scheduler the
/// opportunity to suspend it ans select a different thread to run for a
/// while. It's usually used to avoid busy waiting in user-level threads, when
/// the even we're waiting for isn't an LCO (like user-level lock-based
/// synchronization).
void scheduler_yield(void) {
  // if there's nothing else to do, we can be rescheduled
  thread_t *from = thread_current();
  thread_t *to = _schedule(false, from);
  if (from == to)
    return;

  // transfer to the new thread, using the thread_checkpoint_push() transfer
  // continuation to checkpoint the current stack and to push the current thread
  // onto the self.next epoch list.
  thread_transfer(to->sp, &self.next, thread_checkpoint_push);
}


/// Waits for an LCO to be signaled, by using the _transfer_lco() continuation.
///
/// Uses the "fast" form of _schedule(), meaning that schedule will not try very
/// hard to acquire more work if it doesn't have anything else to do right
/// now. This avoids the situation where this thread is holding an LCO's lock
/// much longer than necessary. Furthermore, _schedule() can't try to select the
/// calling thread because it doesn't know about it (it's not in self.ready or
/// self.next, and it's not passed as the @p final parameter to _schedule).
///
/// We reacquire the lock before returning, which maintains the atomicity
/// requirements for LCO actions.
///
/// @precondition The calling thread must hold @p lco's lock.
void scheduler_wait(lco_t *lco) {
  thread_t *to = _schedule(true, NULL);
  thread_transfer(to->sp, lco, thread_checkpoint_enqueue);
  lco_lock(lco);
}


/// Signals an LCO.
///
/// This uses lco_trigger() to set the LCO and get it its queued threads
/// back. It then goes through the queue and makes all of the queued threads
/// runnable. It does not release the LCO's lock, that must be done by the
/// caller.
///
/// @todo This does not acknowledge locality in any way. We might want to put
///       the woken threads back up into the worker thread where they were
///       running when they waited.
///
/// @precondition The calling thread must hold @p lco's lock.
void scheduler_signal(lco_t *lco) {
  thread_t *q = lco_trigger(lco);
  if (q)
    thread_cat(&self.next, q);
}


/// Exits a user-level thread.
///
/// This releases the underlying parcel, and deletes the thread structure as the
/// transfer continuation. This will never return, because the current thread is
/// put into the _free threads list and not into a runnable list (self.ready,
/// self.next, or an lco).
void scheduler_exit(hpx_parcel_t *parcel) {
  // hpx_parcel_release(parcel);
  thread_t *to = _schedule(false, NULL);
  thread_transfer(to->sp, &self.free, thread_exit_push);
  unreachable();
}


int hpx_get_my_thread_id(void) {
  return self.id;
}
