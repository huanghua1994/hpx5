/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Pingong example
  examples/hpx/pingpong.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <hpx.h>

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return hpx_addr_from_rank(r % n);
}

static hpx_action_t send = 0;

static int send_action(void *args) {
  int n = *(int*)args;
  printf("locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
         hpx_get_my_thread_id(), n);

  if (n-- <= 0) {
    printf("terminating.\n");
    hpx_shutdown(n);
  }

  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(int));
  hpx_parcel_set_target(p, rand_rank());
  hpx_parcel_set_action(p, send);
  hpx_parcel_set_data(p, &n, sizeof(n));
  hpx_parcel_send(p);
  return HPX_SUCCESS;
}

int main(int argc, char * argv[argc]) {
  int n = 0;
  int threads = 0;
  switch (argc) {
   default:
    fprintf(stderr, "Usage: countdown ROUNDS [optional THREADS]\n");
    return -1;
   case (3):
    threads = atoi(argv[2]);
   case (2):
    n = atoi(argv[1]);
    break;
  }

  hpx_config_t config = { .scheduler_threads = threads };
  if (hpx_init(&config)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }
  send = hpx_register_action("send", send_action);
  return hpx_run(send, &n, sizeof(n));
}
