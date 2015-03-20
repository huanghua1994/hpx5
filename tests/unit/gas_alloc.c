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

// Goal of this testcase is to test the HPX Memory Allocation
// 1. hpx_gas_alloc() -- Allocates the global memory.
// 3. hpx_gas_try_pin() -- Performs address translation.
// 4. hpx_gas_unpin() -- Allows an address to be remapped.

#include <stdio.h>
#include <stdlib.h>
#include <hpx/hpx.h>
#include "tests.h"

static const int N = 10;

static HPX_ACTION(gas_alloc, void *UNUSED) {
  printf("Starting the GAS local memory allocation test\n");
  hpx_addr_t local = hpx_gas_alloc(N);

  if (!local) {
    fprintf(stderr, "hpx_gas_alloc returned HPX_NULL\n");
    exit(EXIT_FAILURE);
  }

  if (!hpx_gas_try_pin(local, NULL)) {
    fprintf(stderr, "gas alloc returned non-local memory\n");
    exit(EXIT_FAILURE);
  }

  hpx_gas_free(local, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(gas_calloc, void *UNUSED) {
  printf("Starting the GAS local memory allocation test\n");
  hpx_addr_t local = hpx_gas_calloc(N, sizeof(int));

  if (!local) {
    fprintf(stderr, "hpx_gas_calloc returned HPX_NULL\n");
    exit(EXIT_FAILURE);
  }

  int *buffer = NULL;
  if (!hpx_gas_try_pin(local, (void**)&buffer)) {
    fprintf(stderr, "gas calloc returned non-local memory\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < N; ++i) {
    if (buffer[i] != 0) {
      fprintf(stderr, "gas calloc returned uninitialized memory\n");
      exit(EXIT_FAILURE);
    }
  }

  hpx_gas_unpin(local);
  hpx_gas_free(local, HPX_NULL);
  return HPX_SUCCESS;
}

static int _verify_at(hpx_addr_t addr, int zero) {
  int *buffer = NULL;
  if (!hpx_gas_try_pin(addr, (void**)&buffer)) {
    fprintf(stderr, "address not located at correct locality\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < N; ++i) {
    if (zero && buffer[i] != 0) {
      fprintf(stderr, "gas calloc returned uninitialized memory\n");
      exit(EXIT_FAILURE);
    }
  }

  hpx_gas_unpin(addr);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(INTERRUPT, _verify_at, verify_at, HPX_ADDR);

static HPX_ACTION(gas_alloc_at, void *UNUSED){
  printf("Starting the GAS remote memory allocation test\n");
  int peer = (HPX_LOCALITY_ID + 1) % HPX_LOCALITIES;
  hpx_addr_t addr = hpx_gas_alloc_at_sync(N * sizeof(int), HPX_THERE(peer));
  if (!addr) {
    fprintf(stderr, "failed to allocate memory at %d\n", peer);
    exit(EXIT_FAILURE);
  }

  int zero = 0;
  hpx_call_sync(HPX_THERE(peer), verify_at, NULL, 0, &addr, &zero);
  hpx_addr_t wait = hpx_lco_future_new(0);
  hpx_gas_free(addr, wait);
  hpx_lco_wait(wait);
  hpx_lco_delete(wait, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(gas_calloc_at, void *UNUSED){
  printf("Starting the GAS remote memory allocation test\n");
  int peer = (HPX_LOCALITY_ID + 1) % HPX_LOCALITIES;
  hpx_addr_t addr = hpx_gas_alloc_at_sync(N, HPX_THERE(peer));
  if (!addr) {
    fprintf(stderr, "failed to allocate memory at %d\n", peer);
    exit(EXIT_FAILURE);
  }

  int zero = 1;
  hpx_call_sync(HPX_THERE(peer), verify_at, NULL, 0, &addr, &zero);
  hpx_addr_t wait = hpx_lco_future_new(0);
  hpx_gas_free(addr, wait);
  hpx_lco_wait(wait);
  hpx_lco_delete(wait, HPX_NULL);
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(gas_alloc);
  ADD_TEST(gas_alloc_at);
  ADD_TEST(gas_calloc);
  ADD_TEST(gas_calloc_at);
});