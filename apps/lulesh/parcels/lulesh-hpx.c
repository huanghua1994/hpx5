#include "lulesh-hpx.h"

static hpx_action_t _main          = 0;
static hpx_action_t _advanceDomain = 0;
static hpx_action_t _updateNodalMass = 0;

static int _updateNodalMass_action(Nodal *nodal) {
  hpx_addr_t domain = nodal->address;
  int srcLocalIdx = nodal->srcLocalIdx;
  double *src = nodal->buf;
  int rank = nodal->rank;

  recv_t unpack = RECEIVER[srcLocalIdx];

  hpx_addr_t local = hpx_addr_add(domain, sizeof(Domain)*rank);
  Domain *ld;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  int nx = ld->sizeX + 1;
  int ny = ld->sizeY + 1;
  int nz = ld->sizeZ + 1;

  hpx_lco_sema_p(ld->sem);

  unpack(nx, ny, nz, src, ld->nodalMass, 0);

  hpx_lco_sema_v(ld->sem, HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

void SBN1(hpx_addr_t address,Domain *domain, int index)
{
  hpx_lco_sema_p(domain->sem);

  int i;
  int rank = index;
  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;

  // pack outgoing data
  int nsTF = domain->sendTF[0];
  int *sendTF = &domain->sendTF[1];

  // for completing the entire loop
  hpx_addr_t done = hpx_lco_and_new(nsTF);

  for (i = 0; i < nsTF; i++) {
    int destLocalIdx = sendTF[i];
    // acquire a "large-enough buffer to pack into," NULL first parameter means
    // it comes with the parcel and is managed by the parcel and freed by the
    // system inside of send()
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(Nodal) +
                                         BUFSZ[destLocalIdx]);
    assert(p);

    // "interpret the parcel buffer as a Nodal"
    Nodal *nodal = hpx_parcel_get_data(p);

    send_t pack = SENDER[destLocalIdx];
    pack(nx, ny, nz, domain->nodalMass, nodal->buf);

    // the neighbor this is being sent to
    int srcRemoteIdx = destLocalIdx;
    int srcLocalIdx = 25 - srcRemoteIdx;
    int to_rank = rank - OFFSET[srcLocalIdx];

    nodal->rank = to_rank;
    nodal->srcLocalIdx = srcLocalIdx;
    nodal->address = address;

    hpx_parcel_set_target(p, HPX_THERE(to_rank));
    hpx_parcel_set_action(p, _updateNodalMass);
    // hpx_parcel_set_cont(p, HPX_NULL);
    hpx_parcel_send(p, done);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_lco_sema_v(domain->sem, HPX_NULL);
}

static int _advanceDomain_action(Advance *advance) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  int nx = advance->nx;
  int nDoms = advance->nDoms;
  int maxcycles = advance->maxcycles;
  int cores = advance->cores;
  int index = advance->index;

  int tp = (int) (cbrt(nDoms) + 0.5);

  Init(tp,nx);
  int col = index%tp;
  int row = (index/tp)%tp;
  int plane = index/(tp*tp);
  ld->sem = hpx_lco_sema_new(1);
  SetDomain(index, col, row, plane, nx, tp, nDoms, maxcycles,ld);

  SBN1(local,ld,index);

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(int *input)
{
  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_time_t t1 = hpx_time_now();

  int nDoms, nx, maxcycles, cores, tp, i, j, k;
  nDoms = input[0];
  nx = input[1];
  maxcycles = input[2];
  cores = input[3];

  tp = (int) (cbrt(nDoms) + 0.5);
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    return -1;
  }

  hpx_addr_t domain = hpx_gas_global_alloc(nDoms,sizeof(Domain));
  hpx_addr_t and = hpx_lco_and_new(nDoms);
  Advance advance[nDoms];
  for (k=0;k<nDoms;k++) {
    advance[k].index = k;
    advance[k].nDoms = nDoms;
    advance[k].nx = nx;
    advance[k].maxcycles = maxcycles;
    advance[k].cores = cores;
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k);
    hpx_call(block, _advanceDomain, &advance[k], sizeof(advance[k]), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains,nDoms\n"
          "\t-x, nx\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
}


int main(int argc, char **argv)
{
  hpx_config_t cfg = {
    .cores         = 0,
    .threads       = 0,
    .stack_bytes   = 0,
    .gas           = HPX_GAS_PGAS
  };

  int nDoms, nx, maxcycles,cores;
  // default
  nDoms = 8;
  nx = 15;
  maxcycles = 10;
  cores = 8;
  cfg.cores = cores;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:D:n:x:ih")) != -1) {
    switch (opt) {
      case 'c':
        cfg.cores = atoi(optarg);
        cores = cfg.cores;
        break;
      case 't':
        cfg.threads = atoi(optarg);
        break;
      case 'D':
        cfg.wait = HPX_WAIT;
        cfg.wait_at = HPX_LOCALITY_ALL;
        break;
      case 'd':
        cfg.wait = HPX_WAIT;
        cfg.wait_at = atoi(optarg);
        break;
      case 'n':
        nDoms = atoi(optarg);
        break;
      case 'x':
        nx = atoi(optarg);
        break;
      case 'i':
        maxcycles = atoi(optarg);
        break;
      case 'h':
        usage(stdout);
        return 0;
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  _main      = HPX_REGISTER_ACTION(_main_action);
  _advanceDomain   = HPX_REGISTER_ACTION(_advanceDomain_action);
  _updateNodalMass = HPX_REGISTER_ACTION(_updateNodalMass_action);

  int input[4];
  input[0] = nDoms;
  input[1] = nx;
  input[2] = maxcycles;
  input[3] = cores;
  printf(" Number of domains: %d nx: %d maxcycles: %d cores: %d\n",nDoms,nx,maxcycles,cores);

  return hpx_run(_main, input, 4*sizeof(int));

  return 0;
}

