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
#ifndef LIBHPX_SCHEDULER_H
#define LIBHPX_SCHEDULER_H

/// @file libhpx/scheduler/scheduler.h
/// @brief The internal interface to the scheduler.
///
/// The HPX scheduler is a multithreaded application that provides lighweight
/// threads and local-control-objects (monitor/condition variables). It is
/// designed to work as part of a distributed set of schedulers to support a
/// large-scale, lightweight thread-based application.


#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libsync/lockable_ptr.h>
#include <libsync/queues.h>
#include <libhpx/stats.h>
#include <libhpx/worker.h>

/// Preprocessor define that tells us if the scheduler is cooperative or
/// preemptive. Unused at this point
#define LIBHPX_SCHEDULER_COOPERATIVE 1
//#define LIBHPX_SCHEDULER_PREEMPTIVE 1


/// Forward declarations
/// @{
struct thread;
struct barrier;
struct cvar;
/// @}

/// Typedefs
/// @{
typedef two_lock_queue_t yield_queue_t;
/// @}

/// The scheduler class.
///
/// The scheduler class represents the shared-memory state of the entire
/// scheduling process. It serves as a collection of native worker threads, and
/// a network port, and allows them to communicate with each other and the
/// network.
///
/// It is possible to have multiple scheduler instances active within the same
/// memory space---though it is unclear why we would need or want that at this
/// time---and it is theoretically possible to move workers between schedulers
/// by updating the worker's scheduler pointer and the scheduler's worker
/// table, though all of the functionality that is required to make this work is
/// not implemented.
struct scheduler {
  yield_queue_t       yielded;
  volatile int       shutdown;
  volatile int    next_tls_id;
  int                   cores;
  int               n_workers;
  unsigned int    backoff_max;
  struct barrier     *barrier;
  struct worker      *workers;
  scheduler_stats_t     stats;
};


/// Allocate and initialize a new scheduler.
///
/// @param        cores The number of processors this scheduler will run on.
/// @param      workers The number of worker threads to start.
/// @param   stack_size The size of the stacks to allocate.
/// @param  backoff_max The upper bound for worker backoff.
/// @param   statistics The flag that indicates if statistics are on or off.
///
/// @returns            The scheduler object, or NULL if there was an error.
struct scheduler *scheduler_new(int cores, int workers, int stack_size,
                                unsigned int backoff_max, bool stats)
  HPX_INTERNAL HPX_MALLOC;


/// Finalize and free the scheduler object.
///
/// The scheduler must already have been shutdown with
/// scheduler_shutdown(). Shutting down a scheduler that is active, or was
/// aborted with scheduler_abort(), results in undefined behavior.
///
/// @param    scheduler The scheduler to free.
void scheduler_delete(struct scheduler *scheduler)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Print collected statistics.
///
/// The scheduler should not be running during this call.
///
/// @param    scheduler The scheduler.
void scheduler_dump_stats(struct scheduler *scheduler)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Starts the scheduler.
///
/// This starts all of the low-level scheduler threads. After this call, threads
/// can be spawned using the scheduler_spawn() routine. Parcels for this queue
/// may come from the network, or from the main thread.
///
/// @param    scheduler The scheduler to start.
///
/// @returns            LIBHPX_OK or an error code.
int scheduler_startup(struct scheduler *scheduler)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Stops the scheduler cooperatively.
///
/// This asks all of the threads to shutdown the next time they get a chance to
/// schedule. It is nonblocking.
///
/// @param    scheduler The scheduler to shutdown.
/// @param         code The code to return from scheduler_startup().
void scheduler_shutdown(struct scheduler *scheduler, int code)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Check to see if the scheduler is still running.
///
/// @param    scheduler The scheduler to check.
///
/// @returns            True if the scheduler is running.
int scheduler_running(struct scheduler *scheduler)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Join the scheduler at shutdown.
///
/// This will wait until all of the scheduler's worker threads have shutdown.
///
/// @param    scheduler The scheduler to join.
void scheduler_join(struct scheduler *scheduler)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Stops the scheduler asynchronously.
///
/// This cancels and joins all of the scheduler threads, and then returns. It
/// should only be called by the main thread that called scheduler_startup().
///
/// @param    scheduler The scheduler to abort.
void scheduler_abort(struct scheduler *scheduler)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Spawn a new user-level thread for the parcel.
///
/// @param    parcel The parcel to spawn.
void scheduler_spawn(hpx_parcel_t *p)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Yield a user-level thread.
///
/// This triggers a scheduling event, and possibly selects a new user-level
/// thread to run. If a new thread is selected, this moves the thread into the
/// next local epoch, and also makes the thread available to be stolen within
/// the locality.
void scheduler_yield(void)
  HPX_INTERNAL;


/// Wait for an condition.
///
/// This suspends execution of the current user level thread until the condition
/// is signaled. The calling thread must hold the lock. This releases the lock
/// during the wait, but reacquires it before the user-level thread returns.
///
/// scheduler_wait() will call _schedule() and transfer away from the calling
/// thread.
///
/// @param         lock The lock protecting the condition.
/// @param         cvar The condition we'd like to wait for.
///
/// @returns            LIBHPX_OK or an error
hpx_status_t scheduler_wait(lockable_ptr_t *lock, struct cvar *con)
  HPX_NON_NULL(1, 2) HPX_INTERNAL;


/// Signal a condition.
///
/// The calling thread must hold the lock protecting the condition. This call is
/// synchronous (MESA style) and one waiting thread will be woken up.
///
/// @param         cvar The condition we'd like to signal.
void scheduler_signal(struct cvar *cond)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Signal a condition.
///
/// The calling thread must hold the lock protecting the condition. This call is
/// synchronous (MESA style) and all waiting threads will be woken up.
///
/// @param         cvar The condition we'd like to signal.
void scheduler_signal_all(struct cvar *cvar)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Signal an error condition.
///
/// The calling thread must hold the lock on protecting the condition. This call
/// is synchronous (MESA style) and all of the waiting threads will be woken up,
/// with the return value of HPX_LCO_ERROR. The user-supplied error can be
/// retrieved from the condition.
///
/// @param         cvar The condition we'd like to signal an error on.
/// @param         code The error code to set in the condition.
void scheduler_signal_error(struct cvar *cvar, hpx_status_t code)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Get the parcel bound to the current executing thread.
hpx_parcel_t *scheduler_current_parcel(void)
  HPX_INTERNAL;


/// Get a worker by id.
struct worker *scheduler_get_worker(struct scheduler *sched, int id)
  HPX_NON_NULL(1) HPX_INTERNAL;


#endif // LIBHPX_SCHEDULER_H
