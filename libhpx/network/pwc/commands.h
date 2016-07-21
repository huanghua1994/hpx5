// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_NETWORK_PWC_COMMANDS_H
#define LIBHPX_NETWORK_PWC_COMMANDS_H

#include <hpx/hpx.h>
#include <cstdint>

namespace libhpx {
namespace network {
namespace pwc {

class Command {
 public:
  static Command Nop() {
    return Command();
  }

  static Command ResumeParcel(hpx_parcel_t *p) {
    return Command(RESUME_PARCEL, p);
  }

  static Command ResumeParcelAtSource(hpx_parcel_t *p) {
    return Command(RESUME_PARCEL_SOURCE, p);
  }

  static Command RecvParcel(hpx_parcel_t *p) {
    return Command(RECV_PARCEL, p);
  }

  static Command DeleteParcel(const hpx_parcel_t *p) {
    return Command(DELETE_PARCEL, p);
  }

  static Command SetLCO(hpx_addr_t lco) {
    return Command(LCO_SET, lco);
  }

  static Command SetLCOAtSource(hpx_addr_t lco) {
    return Command(LCO_SET_SOURCE, lco);
  }

  static Command ReloadRequest(size_t bytes) {
    return Command(RELOAD_REQUEST, bytes);
  }

  static Command ReloadReply() {
    return Command(RELOAD_REPLY, 0);
  }

  static Command RendezvousLaunch(hpx_parcel_t *p) {
    return Command(RENDEZVOUS_LAUNCH, p);
  }

  Command() : Command(NOP, UINT64_C(0)) {
  }

  operator bool() const {
    return (op_ != NOP);
  }

  void operator()(unsigned src) const;

  static uint64_t Pack(const Command& command) {
    union {
      Command command;
      uint64_t packed;
    } pack = { command };
    return pack.packed;
  }

  static Command Unpack(uint64_t packed) {
    union {
      uint64_t packed;
      Command command;
    } pack = { packed };
    return pack.command;
  }

 private:

  enum : uint16_t {
    NOP = 0,
    RESUME_PARCEL,
    RESUME_PARCEL_SOURCE,
    DELETE_PARCEL,
    LCO_SET,
    LCO_SET_SOURCE,
    RECV_PARCEL,
    RENDEZVOUS_LAUNCH,
    RELOAD_REQUEST,
    RELOAD_REPLY,
    OP_COUNT
  };

  Command(uint16_t op, uint64_t arg) : arg_(arg), op_(op) {
  }

  template <typename T>
  Command(uint16_t op, T *p) :
      arg_(reinterpret_cast<uintptr_t>(p)), op_(op) {
  }

  void resumeParcel(unsigned src) const;
  void resumeParcelAtSource(unsigned src) const;
  void deleteParcel(unsigned src) const;
  void lcoSet(unsigned src) const;
  void lcoSetAtSource(unsigned src) const;
  void recvParcel(unsigned src) const;
  void rendezvousLaunch(unsigned src) const;
  void reloadRequest(unsigned src) const;
  void reloadReply(unsigned src) const;

  uint64_t arg_ : 48;
  uint64_t  op_ : 16;
};

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_COMMANDS_H
