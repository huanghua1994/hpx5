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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"

#define BUFFER_SIZE 128

/* command line options */
static bool         _text = false;            //!< send text data with the ping
static bool      _verbose = false;            //!< print to the terminal

/* actions */
static hpx_action_t _ping = 0;
static hpx_action_t _pong = 0;

/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: pingponghpx [options] ITERATIONS\n"
          "\t-c, the number of cores to run on\n"
          "\t-t, the number of scheduler threads\n"
          "\t-m, send text in message\n"
          "\t-v, print verbose output \n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

static void _register_actions(void);
static hpx_addr_t _partner(void);

/** the pingpong message type */
typedef struct {
  int id;                                       //!< the message id
  char msg[BUFFER_SIZE];                        //!< the text string (if -t)
} args_t;

/* utility macros */
#define CHECK_NOT_NULL(p, err)                                \
  do {                                                        \
    if (!p) {                                                 \
      fprintf(stderr, err);                                   \
      hpx_shutdown(1);                                        \
    }                                                         \
  } while (0)

#define RANK_PRINTF(format, ...)                                        \
  do {                                                                  \
    if (_verbose)                                                       \
      printf("\t%d,%d: " format, hpx_get_my_rank(), hpx_get_my_thread_id(), \
             __VA_ARGS__);                                              \
  } while (0)

int main(int argc, char *argv[]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dmvh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'm':
      _text = true;
     case 'v':
      _verbose = true;
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    _usage(stderr);
    fprintf(stderr, "\nMissing iteration limit\n");
    return -1;
  }

  args_t args = {
    .id = strtol(argv[0], NULL, 10),
    .msg = "starting pingpong"
  };

  if (args.id == 0) {
    _usage(stderr);
    printf("read ITERATIONS as 0, exiting.\n");
    return -1;
  }

  printf("Running: {iterations: %d}, {message: %d}, {verbose: %d}\n",
         args.id, _text, _verbose);

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  _register_actions();

  const char *network = hpx_get_network_id();

  hpx_time_t start = hpx_time_now();
  e = hpx_run(_ping, &args, sizeof(args));
  double elapsed = (double)hpx_time_elapsed_ms(start);
  double latency = elapsed / (args.id * 2);
  printf("average oneway latency (%s):   %f ms\n", network, latency);
  return e;
}

/**
 * Send a ping message.
 */
static int _action_ping(args_t *args) {
  RANK_PRINTF("received '%s'\n", args->msg);

  // Reuse the message space for the next ping message.
  args->id -= 1;
  if (_text)
    snprintf(args->msg, BUFFER_SIZE, "ping %d from (%d, %d)", args->id,
             hpx_get_my_rank(), hpx_get_my_thread_id());

  // If we completed the number of ping-pong operations that we set out to do,
  // then output the latency and terminate execution.
  if (args->id < 0)
    hpx_shutdown(0);


  // Generate a ping targeting pong.
  hpx_addr_t to = _partner();
  RANK_PRINTF("pinging block (%d,%lu), msg= '%s'\n", to.base_id, to.offset,
              args->msg);

  hpx_parcel_t *p = hpx_parcel_acquire(args, sizeof(*args));
  CHECK_NOT_NULL(p, "Failed to acquire parcel in 'ping' action");
  hpx_parcel_set_action(p, _pong);
  hpx_parcel_set_target(p, to);

  // use synchronous send because we'd need to wait to return anyway, otherwise
  // the args buffer will be cleaned up early
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}


/**
 * Handle a pong action.
 */
static int _action_pong(args_t *args) {
  RANK_PRINTF("received '%s'\n", args->msg);

  // reuse args
  if (_text)
    snprintf(args->msg, BUFFER_SIZE, "pong %d from (%d, %d)", args->id,
             hpx_get_my_rank(), hpx_get_my_thread_id());


  hpx_addr_t to = _partner();
  RANK_PRINTF("ponging block (%d,%lu), msg='%s'\n", to.base_id, to.offset,
              args->msg);

  hpx_parcel_t *p = hpx_parcel_acquire(args, sizeof(*args));
  CHECK_NOT_NULL(p, "Could not allocate parcel in 'pong' action\n");
  hpx_parcel_set_action(p, _ping);
  hpx_parcel_set_target(p, to);

  // use synchronous send because we have to wait to return anyway, or we'll
  // deallocate the args buffer early
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}


/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  _ping = HPX_REGISTER_ACTION(_action_ping);
  _pong = HPX_REGISTER_ACTION(_action_pong);
}


hpx_addr_t _partner(void) {
  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();
  return HPX_THERE((rank) ? 0 : ranks - 1);
}
