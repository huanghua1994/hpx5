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

#include <stdio.h>
#include <hpx/hpx.h>
const int elem_per_blk = 10;
#define ELEMENT int

static int _initialize_handler(ELEMENT *element) {
  fprintf(stdout, "Init the array...\n");
  for (int i = 0, e = elem_per_blk; i < e; ++i) {
    element[i] = (ELEMENT)i;
    fprintf(stdout, "%d %d\n", i, element[i]);
  }
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _initialize, _initialize_handler,
                  HPX_POINTER);

static int _print_gas_handler(void *addr){
  ELEMENT *e = addr;
  printf("user_handler:%d\n", *e);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _print_gas, _print_gas_handler, HPX_POINTER);

static int nested_for_handler(void) {
  fprintf(stdout, "localities: %d\nthreads:%d\n", HPX_LOCALITIES, HPX_THREADS);
  int blk_num = HPX_LOCALITIES;
  size_t blk_size = elem_per_blk * sizeof(ELEMENT);
  //allocate gas
  hpx_addr_t array = hpx_gas_alloc_cyclic(blk_num, blk_size, 0);

  //fill numbers
  int e = hpx_gas_bcast_sync(_initialize, array, blk_num, 0, blk_size);
  if (HPX_SUCCESS != e) {
    return e;
  }

  //perform nested for
  int offset = 0;
  e = hpx_nested_for_sync(_print_gas, 0, blk_num * elem_per_blk - 1, blk_size,
                          offset, sizeof(ELEMENT), 0, NULL, array);
  if (HPX_SUCCESS != e)
    return e;
  hpx_gas_free(array, HPX_NULL);
  hpx_exit(HPX_SUCCESS);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, nested_for, nested_for_handler);

int main(int argc, char *argv[argc]) {
  if (hpx_init(&argc, &argv) != 0) {
    return -1;
  }

  int e = hpx_run(&nested_for);
  if (HPX_SUCCESS != e) {
    printf("something failed: %d\n", e);
  }

  hpx_finalize();
  return e;
}