
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness
  00_hpxtest.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#include <stdio.h>

#include <check.h>

/* Source files containing tests */
/* This is suboptimal. So sue me. Actually, please don't. */
#include "01_init.c"
#include "02_mem.c"
#include "03_ctx.c"
#include "04_thread1.c"
#include "05_queue.c"
#include "06_kthread.c"
#include "07_mctx.c"
#include "08_thread2.c"
#include "09_config.c"


/*
 --------------------------------------------------------------------
  Main
 --------------------------------------------------------------------
*/

int main(int argc, char * argv[]) {
  Suite * s = suite_create("hpxtest");
  TCase * tc = tcase_create("hpxtest-core");
  char * long_tests = NULL;
  char * hardcore_tests = NULL;

  /* figure out if we need to run long-running tests */
  long_tests = getenv("HPXTEST_EXTENDED");

  /* see if we're in HARDCORE mode (heh) */
  hardcore_tests = getenv("HPXTEST_HARDCORE");

  /* install fixtures */
  tcase_add_checked_fixture(tc, hpxtest_core_setup, hpxtest_core_teardown);

  /* set timeout */
  tcase_set_timeout(tc, 1200);

  /* test memory management */
  tcase_add_test(tc, test_libhpx_alloc);

  /* test kernel threads */
  /* NOTE: should come before context tests */
  tcase_add_test(tc, test_libhpx_kthread_get_cores);
  tcase_add_test(tc, test_libhpx_kthread_create);

  /* test scheduling context management */
  tcase_add_test(tc, test_libhpx_ctx_create);
  tcase_add_test(tc, test_libhpx_ctx_get_id);

  /* test threads (stage 1) */
  //  tcase_add_test(tc, test_libhpx_thread_create);

  /* test FIFO queues */
  tcase_add_test(tc, test_libhpx_queue_size);
  tcase_add_test(tc, test_libhpx_queue_insert);
  tcase_add_test(tc, test_libhpx_queue_peek);
  tcase_add_test(tc, test_libhpx_queue_pop);

  /* test machine context switching */
  tcase_add_test(tc, test_libhpx_mctx_getcontext);
  tcase_add_test(tc, test_libhpx_mctx_getcontext_ext);
  tcase_add_test(tc, test_libhpx_mctx_getcontext_sig);
  tcase_add_test(tc, test_libhpx_mctx_getcontext_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_setcontext);
  tcase_add_test(tc, test_libhpx_mctx_setcontext_ext);
  tcase_add_test(tc, test_libhpx_mctx_setcontext_sig);
  tcase_add_test(tc, test_libhpx_mctx_setcontext_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_0arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_1arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_2arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_3arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_4arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_5arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_6arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_7arg);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_8arg);  
  tcase_add_test(tc, test_libhpx_mctx_makecontext_0arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_1arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_2arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_3arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_4arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_5arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_6arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_7arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_8arg_ext);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_0arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_1arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_2arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_3arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_4arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_5arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_6arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_7arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_8arg_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_0arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_1arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_2arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_3arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_4arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_5arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_6arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_7arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_makecontext_8arg_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain1);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain2);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain310);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain311);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain312);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain8000);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_chain90000);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star2);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star10);

  if (long_tests) {
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1000);
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star5000);
  }

  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1_ext);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star2_ext);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star10_ext);

  if (long_tests) {
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1000_ext);
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star5000_ext);
  }

  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1_sig);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star2_sig);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star10_sig);

  if (long_tests) {
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1000_sig);
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star5000_sig);
  }

  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star2_ext_sig);
  tcase_add_test(tc, test_libhpx_mctx_swapcontext_star10_ext_sig);

  if (long_tests) {
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star1000_ext_sig);
    tcase_add_test(tc, test_libhpx_mctx_swapcontext_star5000_ext_sig);
  }

  /* test LCOs */
  tcase_add_test(tc, test_libhpx_lco_futures);

  /* test threads (stage 2) */
  tcase_add_test(tc, test_libhpx_thread_self_ptr);
  tcase_add_test(tc, test_libhpx_thread_self_ptr_ext);
  tcase_add_test(tc, test_libhpx_thread_self_ptr_sig);
  tcase_add_test(tc, test_libhpx_thread_self_ptr_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1_ext);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1_sig);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_args);
  tcase_add_test(tc, test_libhpx_thread_args_ext);
  tcase_add_test(tc, test_libhpx_thread_args_sig);
  tcase_add_test(tc, test_libhpx_thread_args_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2_ext);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2_sig);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield1);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield2);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_x2);

  if (long_tests || hardcore_tests) {
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32_ext);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32_sig);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32_ext_sig);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_x32);
  }

  if (hardcore_tests) {
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_hardcore1000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_hardcore5000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_hardcore1000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_hardcore5000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_hardcore10000);
  }

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  
  return (failed == 0) ? 0 : -1;
}
