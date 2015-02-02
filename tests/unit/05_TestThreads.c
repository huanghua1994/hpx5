//****************************************************************************
// @Filename      05_TestThreads.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - HPX Thread interface
//
// @Compiler      GCC
// @OS            Linux
// @Description   Tests the thread functions - thread.h
// @Goal          Goal of this testcase is to test HPX thread interface
//                1. hpx_thread_current_target()
//                2. hpx_thread_current_cont_target()
//                3. hpx_thread_current_cont_action()
//                4. hpx_thread_current_args_size()
//                5. hpx_thread_yield()
//                6. hpx_thread_get_tls_id()
//                7. hpx_thread_set_affinity()
//                8. hpx_thread_continue()
//                9. hpx_thread_continue_cleanup()
//               10. hpx_thread_exit()
//
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          08/22/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "domain.h"
#include "libhpx/locality.h"
#include "libsync/queues.h"

#define NUM_THREADS 5
#define ARRAY_SIZE 100

const int DATA_SIZE = sizeof(uint64_t);
const int SET_CONT_VALUE = 1234;

int t05_initData_action(const initBuffer_t *args)
{
 // Get the target of the current thread. The target of the thread is the
 // destination that a parcel was sent to to spawn the current thread.
 // hpx_thread_current_target() returns the address of the thread's target
  hpx_addr_t local = hpx_thread_current_target();
  initBuffer_t *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->index = args->index;
  strcpy(ld->message, args->message);

  //Get the size of the arguments passed to the current thread
  //uint32_t size = hpx_thread_current_args_size();

  hpx_gas_unpin(local);
  //printf("Initialized buffer with index: %u, with message: %s, size of arguments = %d\n", ld->index, ld->message, size);
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- ThreadCreate
//****************************************************************************
START_TEST (test_libhpx_threadCreate)
{
  fprintf(test_log, "Starting the Threads test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t addr = hpx_gas_global_alloc(NUM_THREADS, sizeof(initBuffer_t));
  hpx_addr_t done = hpx_lco_and_new(NUM_THREADS);

  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync().
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));

    // Fill the buffer
    initBuffer_t *init = hpx_parcel_get_data(p);
    init->index = t;
    strcpy(init->message, "Thread creation test");

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, hpx_addr_add(addr, sizeof(initBuffer_t) * t, sizeof(initBuffer_t)));
    hpx_parcel_set_action(p, t05_initData);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_free(addr, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Finish the current thread's execution. The behavior of this call depends
// on the status parameter, and is equivalent to returning status from
// the action.
// HPX_SUCCESS: Normal termination, send a parcel with 0-sized data to the
// the thread's continuation address.
//****************************************************************************
int t05_worker_action(uint64_t *args)
{
  //uint64_t n;
  //n = *(uint64_t*)args;

  //printf("Value of n =  %"PRIu64" \n", n);
  hpx_thread_exit(HPX_LCO_ERROR);
}

START_TEST (test_libhpx_threadExit)
{
  fprintf(test_log, "Starting the Thread Exit test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  uint64_t value = SET_CONT_VALUE;
  hpx_status_t status = hpx_call(HPX_HERE, t05_worker, done, &value, sizeof(value));
  ck_assert_msg(status == HPX_SUCCESS, "Could not normally terminate the thread");
  hpx_lco_wait(done);

  hpx_lco_get(done, sizeof(uint64_t), &value);
  ck_assert(value == HPX_SUCCESS);

  hpx_lco_delete(done, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST


//****************************************************************************
// hpx_thread_get_tls_id() Generates a consecutive new ID for a thread
// The first time this is called in a leightweight thread, it assigns the next
// available ID. Each time it's called after that it returns the same ID.
//****************************************************************************
int t05_assignID_action(void *args)
{
  hpx_thread_get_tls_id();
  hpx_thread_get_tls_id();
 // int tid = hpx_thread_get_tls_id();
 // int consecutiveID = hpx_thread_get_tls_id();
  //printf("First time generated ID: %d, consecutive new ID:  %d\n", tid,
  //                                     consecutiveID);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_threadGetTlsID)
{
  fprintf(test_log, "Starting the Threads ID generation test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_THREADS);

  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync().
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, HPX_THERE(t % hpx_get_num_ranks()));
    hpx_parcel_set_action(p, t05_assignID);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address.
//****************************************************************************

int t05_set_cont_action(void *args) {
  uint64_t value = SET_CONT_VALUE;
  hpx_thread_continue(DATA_SIZE, &value);
}

START_TEST (test_libhpx_threadContinue)
{
  fprintf(test_log, "Starting the Thread continue test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t *cont_fut = calloc(hpx_get_num_ranks(), sizeof(hpx_addr_t));

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    cont_fut[i] = hpx_lco_future_new(DATA_SIZE);
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);
    hpx_parcel_set_target(p, HPX_THERE(i));
    hpx_parcel_set_action(p, t05_cont_thread);
    hpx_parcel_set_cont_target(p, cont_fut[i]);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send(p, HPX_NULL);
    fprintf(test_log, "Sending action with continuation to %d\n", i);
  }

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    uint64_t result;
    fprintf(test_log, "Waiting on continuation to %d\n", i);
    hpx_lco_get(cont_fut[i], DATA_SIZE, &result);
    fprintf(test_log, "Received continuation from %d with value %" PRIu64 "\n", i, result);
    assert(result == SET_CONT_VALUE);
    hpx_lco_delete(cont_fut[i], HPX_NULL);
  }

  free(cont_fut);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// hpx_thread_yield()
//****************************************************************************

struct t05_yield_args {
  size_t *counter;
  size_t limit;
  double time_limit;
};

int t05_yield_worker_action(void *vargs) {
  struct t05_yield_args *args = (struct t05_yield_args*)vargs;

  // int num =
  sync_addf(args->counter, 1, SYNC_SEQ_CST);

  uint64_t timeout = false;
  hpx_time_t start_time = hpx_time_now();
  while (*args->counter < args->limit) {
    if (hpx_time_elapsed_ms(start_time) > args->time_limit) {
      timeout = true;
      break;
    }
    hpx_thread_yield();
    // printf("Thread %d yielding after %f ms.\n", num, hpx_time_elapsed_ms(start_time));
  }

  // printf("Thread %d done after %f ms.\n", num, hpx_time_elapsed_ms(start_time));
  hpx_thread_continue(sizeof(uint64_t), &timeout);
}

START_TEST (test_libhpx_threadYield)
{
  int num_threads = hpx_get_num_threads();
  size_t counter = 0;

  struct t05_yield_args args = {
    .counter = &counter,
    .limit = num_threads + 1,
    .time_limit = 5000.0
  };
  hpx_addr_t *done = malloc(sizeof(hpx_addr_t) * (num_threads + 1));


  // now spawn num_threads + 1 num_threads
  for (int i = 0; i < num_threads + 1; i++) {
    done[i] = hpx_lco_future_new(sizeof(uint64_t));
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(args));
    hpx_parcel_set_action(p, t05_yield_worker);
    hpx_parcel_set_target(p, HPX_HERE);
    hpx_parcel_set_data(p, &args, sizeof(args));
    hpx_parcel_set_cont_target(p, done[i]);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send(p, HPX_NULL);
  }

  // wait on all done[]s. if any are true, yield() failed
  uint64_t any_timeouts = false;
  uint64_t timeout = false;
  for (int i = 0; i < num_threads + 1; i++) {
    hpx_lco_get(done[i], sizeof(timeout), &timeout);
    any_timeouts |= timeout;
    hpx_lco_delete(done[i], HPX_NULL);
  }
  ck_assert_msg(any_timeouts == false, "Threads did not yield.");

  free(done);
}
END_TEST


//****************************************************************************
// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address. This version gives the
// application a chance to cleanup for instance, to free the value. After
// dealing with the continued data, it will run cleanup(env).
//****************************************************************************
int t05_thread_cont_cleanup_action(void *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  uint64_t local;
  if (!hpx_gas_try_pin(addr, (void**)&local))
    return HPX_RESEND;

  local = SET_CONT_VALUE;
  uint64_t *value = malloc(sizeof(uint64_t));
  *value = local;

  hpx_gas_unpin(addr);
  hpx_thread_continue_cleanup(DATA_SIZE, value, free, value);
}

START_TEST (test_libhpx_threadContinueCleanup)
{
  fprintf(test_log, "Starting the Thread continue cleanup test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t src = hpx_gas_alloc(sizeof(uint64_t));
  int rank = hpx_get_my_rank();

  uint64_t *block = malloc(DATA_SIZE);
  assert(block);

  hpx_call_sync(src, t05_thread_cont_cleanup, block, DATA_SIZE, &rank, sizeof(rank));
  fprintf(test_log, "value in block is %"PRIu64"\n", *block);

  free(block);
  hpx_gas_free(src, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// hpx_thread_current_cont_action gets the continuation action for the current
// thread
//****************************************************************************
static HPX_ACTION(t05_thread_current_cont_target, void *args) {
  hpx_action_t c_action = hpx_thread_current_cont_action();
  hpx_addr_t c_target = hpx_thread_current_cont_target();
  hpx_call(c_target, c_action, HPX_NULL, NULL, 0);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_threadContAction)
{
  fprintf(test_log, "Starting the Thread continue target and action test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t *cont_and = calloc(hpx_get_num_ranks(), sizeof(hpx_addr_t));

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    cont_and[i] = hpx_lco_and_new(2);
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, DATA_SIZE);
    hpx_parcel_set_target(p, HPX_THERE(i));
    hpx_parcel_set_action(p, t05_thread_current_cont_target);
    hpx_parcel_set_cont_target(p, cont_and[i]);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send_sync(p);
    fprintf(test_log, "Started index %d.\n", i);
  }

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    hpx_lco_wait(cont_and[i]);
    fprintf(test_log, "Received continuation from %d\n",i);
    hpx_lco_delete(cont_and[i], HPX_NULL);
  }

  free(cont_and);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_05_TestThreads(TCase *tc) {
  tcase_add_test(tc, test_libhpx_threadCreate);
  tcase_add_test(tc, test_libhpx_threadExit);
  tcase_add_test(tc, test_libhpx_threadGetTlsID);
  tcase_add_test(tc, test_libhpx_threadContinue);
  tcase_add_test(tc, test_libhpx_threadContinueCleanup);
  tcase_add_test(tc, test_libhpx_threadContAction);
  tcase_add_test(tc, test_libhpx_threadYield);
}
