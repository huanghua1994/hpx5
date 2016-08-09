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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "PWCNetwork.h"
#include "DMAStringOps.h"
#include "libhpx/collective.h"
#include "libhpx/gpa.h"
#include "libhpx/libhpx.h"
#include "libhpx/ParcelStringOps.h"
#include <exception>

namespace {
using libhpx::CollectiveOps;
using libhpx::LCOOps;
using libhpx::MemoryOps;
using libhpx::ParcelOps;
using libhpx::StringOps;
using libhpx::network::ParcelStringOps;
using libhpx::network::pwc::PWCNetwork;
using libhpx::network::pwc::DMAStringOps;
using libhpx::network::pwc::ReloadParcelEmulator;
using Op = libhpx::network::pwc::PhotonTransport::Op;
using Key = libhpx::network::pwc::PhotonTransport::Key;
constexpr int ANY_SOURCE = libhpx::network::pwc::PhotonTransport::ANY_SOURCE;
}

PWCNetwork* PWCNetwork::Instance_ = nullptr;

PWCNetwork& PWCNetwork::Instance()
{
  assert(Instance_);
  return *Instance_;
}

PWCNetwork::PWCNetwork(const config_t *cfg, boot_t *boot, gas_t *gas)
    : ReloadParcelEmulator(cfg, boot, gas),
      rank_(boot_rank(boot)),
      ranks_(boot_n_ranks(boot)),
      string_((gas->type == HPX_GAS_AGAS) ?
              static_cast<StringOps*>(new ParcelStringOps()) :
              static_cast<StringOps*>(new DMAStringOps(*this,
                                                       boot_rank(boot)))),
      gas_(gas),
      boot_(boot),
      progressLock_(),
      probeLock_()
{
  assert(!Instance_);
  Instance_ = this;

  // Validate parameters.
  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate PWC for the SMP boot network\n");
    throw std::exception();
  }

  // Validate configuration.
  if (popcountl(cfg->pwc_parcelbuffersize) != 1) {
    dbg_error("--hpx-pwc-parcelbuffersize must 2^k (given %zu)\n",
              cfg->pwc_parcelbuffersize);
  }

  if (cfg->pwc_parceleagerlimit > cfg->pwc_parcelbuffersize) {
    dbg_error(" --hpx-pwc-parceleagerlimit (%zu) must be less than "
              "--hpx-pwc-parcelbuffersize (%zu)\n",
              cfg->pwc_parceleagerlimit, cfg->pwc_parcelbuffersize);
  }
}

PWCNetwork::~PWCNetwork()
{
  // Cleanup any remaining local work---this can leak memory and stuff, because
  // we aren't actually running the commands that we cleanup.
  {
    std::lock_guard<std::mutex> _(progressLock_);
    int remaining, src;
    Command command;
    do {
      PhotonTransport::Test(&command, &remaining, ANY_SOURCE, &src);
    } while (remaining > 0);
  }

  // Network deletion is effectively a collective, so this enforces that
  // everyone is done with rdma before we go and deregister anything.
  boot_barrier(boot_);
  delete string_;
  Instance_ = nullptr;
}

int
PWCNetwork::type() const
{
  return HPX_NETWORK_PWC;
}

void
PWCNetwork::progress(int n)
{
  if (auto _ = std::unique_lock<std::mutex>(progressLock_, std::try_to_lock)) {
    Command command;
    int src;
    while (PhotonTransport::Test(&command, nullptr, ANY_SOURCE, &src)) {
      command(rank_);
    }
  }
}

void
PWCNetwork::flush()
{
}

hpx_parcel_t*
PWCNetwork::probe(int n)
{
  if (auto _ = std::unique_lock<std::mutex>(probeLock_, std::try_to_lock)) {
    Command command;
    int src;
    while (PhotonTransport::Probe(&command, nullptr, ANY_SOURCE, &src)) {
      command(src);
    }
  }
  return nullptr;
}

CollectiveOps&
PWCNetwork::collectiveOpsProvider()
{
  return *this;
}

LCOOps&
PWCNetwork::lcoOpsProvider()
{
  return *this;
}

MemoryOps&
PWCNetwork::memoryOpsProvider()
{
  return *this;
}

ParcelOps&
PWCNetwork::parcelOpsProvider()
{
  return *this;
}

StringOps&
PWCNetwork::stringOpsProvider()
{
  return *string_;
}

void
PWCNetwork::deallocate(const hpx_parcel_t* p)
{
  InplaceBlock::DeleteParcel(p);
}

int
PWCNetwork::send(hpx_parcel_t *p, hpx_parcel_t *ssync)
{
  // This is a blatant hack to keep track of the ssync parcel using p's next
  // pointer. It will allow us to both delete p and run ssync once the
  // underlying network operation is serviced. It works in conjunction with the
  // handle_delete_parcel command.
  dbg_assert(p->next == NULL);
  p->next = ssync;

  if (parcel_size(p) >= here->config->pwc_parceleagerlimit) {
    return rendezvousSend(p);
  }
  else {
    int rank = gas_owner_of(gas_, p->target);
    ends_[rank].send(p);
    return HPX_SUCCESS;
  }
}

void
PWCNetwork::progressSends(unsigned rank)
{
  ends_[rank].progress();
}

void
PWCNetwork::pin(const void *base, size_t bytes, void *key)
{
  PhotonTransport::Pin(base, bytes, static_cast<Key*>(key));
}

void
PWCNetwork::unpin(const void *base, size_t bytes)
{
  PhotonTransport::Unpin(base, bytes);
}

int
PWCNetwork::init(void **collective)
{
  return LIBHPX_OK;
}

int
PWCNetwork::sync(void *in, size_t in_size, void* out, void *collective)
{
  return LIBHPX_OK;
}

void
PWCNetwork::put(hpx_addr_t dest, const void *src, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  int rank = gpa_to_rank(dest);
  ends_[rank].put(dest, src, n, lcmd, rcmd);
}

void
PWCNetwork::get(void *dest, hpx_addr_t src, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  int rank = gpa_to_rank(src);
  ends_[rank].get(dest, src, n, lcmd, rcmd);
}

void*
PWCNetwork::operator new (size_t size)
{
  PhotonTransport::Initialize(here->config, here->boot);
  void *memory;
  if (posix_memalign(&memory, HPX_CACHELINE_SIZE, size)) {
    dbg_error("Could not allocate aligned memory for the PWCNetwork\n");
    throw std::bad_alloc();
  }
  return memory;
}

void
PWCNetwork::operator delete (void *p)
{
  free(p);
}