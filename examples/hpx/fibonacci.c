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
#include <argp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <hpx.h>

/// The command line arguments for fibonacci
typedef struct {
  int n;
  int debug;
  int threads;
} args_t;

/// The options that fibonacci understands.
static struct argp_option opts[] = {
  {"debug", 'd', 0, 0, "Wait for the debugger"},
  {"threads", 't', "HPX_THREADS", 0, "HPX scheduler threads"},
  { 0 }
};

static char doc[] = "Fibonnaci: A simple fibonacci example using HPX.";
static char args_doc[] = "ARG1";

// Our argument parser.
static int parse(int key, char *arg, struct argp_state *state) {
  args_t *args = (args_t*)state->input;

  switch (key) {
   default:
    return ARGP_ERR_UNKNOWN;

   case 'd':
    args->debug = 1;
    break;

   case 't':
    args->threads = atoi(arg);
    break;

   case ARGP_KEY_NO_ARGS:
    argp_usage(state);
    break;

   case ARGP_KEY_ARG:
    if (state->arg_num > 1)
      argp_usage(state);

    if (!arg)
      abort();

    args->n = atoi(arg);
    break;
  }
  return 0;
}

static hpx_action_t fib = 0;
static hpx_action_t fib_main = 0;

static int
fib_action(void *args) {
  int n = *(int*)args;

  if (n < 2)
    hpx_thread_exit(HPX_SUCCESS, &n, sizeof(n));

  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();

  hpx_addr_t peers[] = {
    hpx_addr_from_rank((rank + ranks - 1) % ranks),
    hpx_addr_from_rank((rank + 1) % ranks)
  };

  int ns[] = {
    n - 1,
    n - 2
  };

  hpx_addr_t futures[] = {
    hpx_future_new(sizeof(int)),
    hpx_future_new(sizeof(int))
  };

  int fns[] = {
    0,
    0
  };

  void *addrs[] = {
    &fns[0],
    &fns[1]
  };

  int sizes[] = {
    sizeof(int),
    sizeof(int)
  };

  hpx_call(peers[0], fib, &ns[0], sizeof(int), futures[0]);
  hpx_call(peers[1], fib, &ns[1], sizeof(int), futures[1]);
  hpx_future_get_all(2, futures, addrs, sizes);
  hpx_future_delete(futures[0]);
  hpx_future_delete(futures[1]);

  int fn = fns[0] + fns[1];
  hpx_thread_exit(HPX_SUCCESS, &fn, sizeof(fn));
  return HPX_SUCCESS;
}

static int
fib_main_action(void *args) {
  int n = *(int*)args;
  int fn = 0;                                   // fib result
  printf("fib(%d)=", n); fflush(stdout);
  hpx_time_t clock = hpx_time_now();
  hpx_addr_t future = hpx_future_new(sizeof(int));
  hpx_call(hpx_addr_from_rank(hpx_get_my_rank()), fib, &n, sizeof(n), future);
  hpx_future_get(future, &fn, sizeof(fn));

  double time = hpx_time_elapsed_ms(clock)/1e3;

  printf("%d\n", fn);
  printf("seconds: %.7f\n", time);
  printf("localities:   %d\n", hpx_get_num_ranks());
  hpx_shutdown(0);
  return HPX_SUCCESS;
}

int main(int argc, char *argv[]) {
  args_t args = {0};
  struct argp parser = { opts, parse, args_doc, doc };
  int e = argp_parse(&parser, argc, argv, 0, 0, &args);
  if (e)
    return e;

  hpx_config_t config = {
    .scheduler_threads = args.threads,
    .stack_bytes = 0
  };

  if (args.debug) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(5);
  }

  e = hpx_init(&config);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the fib action
  fib = hpx_action_register("fib", fib_action);
  fib_main = hpx_action_register("fib_main", fib_main_action);

  // run the main action
  return hpx_run(fib_main, &args.n, sizeof(args.n));
}
