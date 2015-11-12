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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>

#include "inst.h"

typedef struct {
  network_t vtable;
  network_t *impl;
} _inst_network_t;

// Define the transports allowed for the SMP network
static void _inst_delete(void *network) {
  _inst_network_t *inst = network;
  inst->impl->delete(network);
  free(inst);
}

static int _inst_progress(void *network, int id) {
  _inst_network_t *inst = network;
  INST(uint64_t start_time = hpx_time_to_ns(hpx_time_now()));
  int r = inst->impl->progress(network, id);
  inst_trace(HPX_INST_SCHEDTIMES, HPX_INST_SCHEDTIMES_PROGRESS, start_time);
  return r;
}

static int _inst_send(void *network, hpx_parcel_t *p) {
  _inst_network_t *inst = network;
  return inst->impl->send(network, p);
}

static int _inst_command(void *network, hpx_addr_t rank,
                        hpx_action_t op, uint64_t args) {
  _inst_network_t *inst = network;
  return inst->impl->command(network, rank, op, args);
}

static int _inst_pwc(void *network,
                    hpx_addr_t to, const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr,
                    hpx_action_t rop, hpx_addr_t raddr) {
  _inst_network_t *inst = network;
  return inst->impl->pwc(network, to, from, n, lop, laddr, rop, raddr);
}

static int _inst_put(void *network, hpx_addr_t to,
                    const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  _inst_network_t *inst = network;
  return inst->impl->put(network, to, from, n, lop, laddr);
}

static int _inst_get(void *network, void *to, hpx_addr_t from, size_t n,
                     hpx_action_t lop, hpx_addr_t laddr) {
  _inst_network_t *inst = network;
  return inst->impl->get(network, to, from, n, lop, laddr);
}

static hpx_parcel_t *_inst_probe(void *network, int nrx) {
  _inst_network_t *inst = network;
  INST(uint64_t start_time = hpx_time_to_ns(hpx_time_now()));
  hpx_parcel_t *p = inst->impl->probe(network, nrx);
  inst_trace(HPX_INST_SCHEDTIMES, HPX_INST_SCHEDTIMES_PROBE, start_time);
  return p;
}

static void _inst_set_flush(void *network) {
  _inst_network_t *inst = network;
  inst->impl->set_flush(network);
}

static void _inst_register_dma(void *network, const void *addr, size_t n,
                               void *key) {
  _inst_network_t *inst = network;
  inst->impl->register_dma(network, addr, n, key);
}

static void _inst_release_dma(void *network, const void *addr, size_t n) {
  _inst_network_t *inst = network;
  inst->impl->release_dma(network, addr, n);
}

static int _inst_lco_wait(void *network, hpx_addr_t lco, int reset) {
  _inst_network_t *inst = network;
  return inst->impl->lco_wait(network, lco, reset);
}

static int _inst_lco_get(void *network, hpx_addr_t lco, size_t n, void *to,
                         int reset) {
  _inst_network_t *inst = network;
  return inst->impl->lco_get(network, lco, n, to, reset);
}

network_t *network_inst_new(network_t *impl) {
  dbg_assert(impl);
  _inst_network_t *inst = malloc(sizeof(*inst));
  dbg_assert(inst);

  inst->vtable.type = impl->type;
  inst->vtable.delete = _inst_delete;
  inst->vtable.progress = _inst_progress;
  inst->vtable.send = _inst_send;
  inst->vtable.command = _inst_command;
  inst->vtable.pwc = _inst_pwc;
  inst->vtable.put = _inst_put;
  inst->vtable.get = _inst_get;
  inst->vtable.probe = _inst_probe;
  inst->vtable.set_flush = _inst_set_flush;
  inst->vtable.register_dma = _inst_register_dma;
  inst->vtable.release_dma = _inst_release_dma;
  inst->vtable.lco_get = _inst_lco_get;
  inst->vtable.lco_wait = _inst_lco_wait;

  inst->impl = impl;

  return &inst->vtable;
}