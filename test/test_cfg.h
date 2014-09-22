#include "photon.h"
#include <mpi.h>

struct photon_config_t cfg = {
  .nproc = 0,
  .address = 0,
  .forwarder = {
    .use_forwarder = 0
  },
  .ibv = {
    .use_cma = 0,
    .use_ud = 0,
    .eth_dev = "roce0",
    .ib_dev = "qib0",
    .ib_port = 1,
  },
  .ugni = {
    .bte_thresh = -1,
  },
  .cap = {
    .small_msg_size = -1,
    .small_pwc_size =  128,
    .eager_buf_size = -1,
    .ledger_entries = -1
  },
  .meta_exch = PHOTON_EXCH_MPI,
  .comm = MPI_COMM_WORLD,
  .backend = "verbs"
};
