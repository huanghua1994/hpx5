#include <stdio.h>
#include <unistd.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"
#include "timer.h"

// the fortran entry point
extern int lulesh(int nx, int its);

static hpx_action_t _lulesh = 0;
static hpx_action_t _hpxmain = 0;

static int _lulesh_action(int args[2] /* nx, its */) {
  int err;
  mpi_init_(&err);
  lulesh(args[0], args[1]);
  mpi_finalize_(&err);
  return HPX_SUCCESS;
}

static int _hpxmain_action(int args[3] /* hpxranks, nx, its */) {
  mpi_system_init(args[0], 0);

  hpx_addr_t and = hpx_lco_and_new(args[0]);
  for (int i = 0, e = hpx_get_num_ranks(); i < e; ++i) {
    int size = args[0];
    int ranks_there = 0; // important to be 0
    get_ranks_per_node(i, &size, &ranks_there, NULL);

    for (int j = 0; j < ranks_there; ++j)
      hpx_call(HPX_THERE(i), _lulesh, &args[1], 2 * sizeof(int), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  mpi_system_shutdown();
  hpx_shutdown(0);
}

hpx_timer_t ts;

void start_time()
{
  hpx_get_time(&ts);
}

double etime()
{
  double elapsed = hpx_elapsed_us(ts);
  return elapsed;
}

void mpi_barrier_( MPI_Comm *pcomm,int *pier)
{
}

int main(int argc, char *argv[])
{

   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   printf("PID %d on %s ready for attach\n", getpid(), hostname);
   fflush(stdout);
   //   sleep(8);

  if ( argc < 5 ) {
    printf(" Usage: depicx <number of OS threads> <number of hpx threads (must be a power of 3)> <lulesh nx> <lulesh iterations>\n");
    printf("        (Hint: for testing try 8 hpx threads, nx = 24, iterations = 10 for testing)\n");
    exit(0);
  }

  uint64_t numos = atoll(argv[1]);
  int numhpx = atoi(argv[2]);
  int nx = atoi(argv[3]);
  int its = atoi(argv[4]);

  printf(" Number OS threads: %ld Number lightweight threads: %d nx: %d its: %d\n",numos,numhpx,nx,its);

  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  cfg.cores = numos;
  cfg.threads = numos;
  // cfg.stack_bytes = 2<<24

  int error = hpx_init(&cfg);
  if (error != HPX_SUCCESS)
    exit(-1);

  mpi_system_register_actions();

  // register local actions
  _lulesh = HPX_REGISTER_ACTION(_lulesh_action);
  _hpxmain = HPX_REGISTER_ACTION(_hpxmain_action);

  // do stuff here
  int args[3] = { numhpx, nx, its };
  hpx_run(_hpxmain, args, sizeof(args));

  return 0;

}
