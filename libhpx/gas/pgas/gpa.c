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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include "heap.h"
#include "gpa.h"
#include "pgas.h"

/// Compute the phase (offset within block) of a global address.
static uint32_t _phase_of(hpx_addr_t gpa, uint32_t bsize) {
  // the phase is stored in the least significant bits of the gpa, we merely
  // mask it out
  //
  // before: (locality, offset, phase)
  //  after: (00000000  000000  phase)
  const uint64_t mask = (1ul << ceil_log2_32(bsize)) - 1;
  return (uint32_t)(gpa & mask);
}


/// Compute the block ID for a global address.
static uint64_t _block_of(hpx_addr_t gpa, uint32_t bsize) {
  // clear the upper bits by shifting them out, and then shifting the offset
  // down to the right place
  //
  // before: (locality, block, phase)
  //  after: (00000000000      block)
  const uint32_t rshift = ceil_log2_32(bsize);
  return (gpa & GPA_OFFSET_MASK) >> rshift;
}


static hpx_addr_t _triple_to_gpa(uint32_t rank, uint64_t bid, uint32_t phase,
                                 uint32_t bsize) {
  // make sure that the phase is in the expected range, locality will be checked
  DEBUG_IF (bsize && phase) {
    if (phase >= bsize) {
      dbg_error("phase %u must be less than %u\n", phase, bsize);
    }
  }

  DEBUG_IF (!bsize && phase) {
    dbg_error("cannot initialize a non-cyclic gpa with a phase of %u\n", phase);
  }

  // forward to pgas_offset_to_gpa(), by computing the offset by combining bid
  // and phase
  const uint32_t shift = (bsize) ? ceil_log2_32(bsize) : 0;
  const uint64_t offset = (bid << shift) + phase;
  return pgas_offset_to_gpa(rank, offset);
}


/// Forward declaration to be used in checking of address difference.
static hpx_addr_t _pgas_gpa_add_cyclic(hpx_addr_t gpa, int64_t bytes, uint32_t bsize, bool debug);

/// Implementation of address difference to be called from the public interface.
/// The debug parameter is used to stop recursion in debugging checks.
static int64_t _pgas_gpa_sub_cyclic(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize, bool debug) {
  // for a block cyclic computation, we look at the three components
  // separately, and combine them to get the overall offset
  const uint32_t plhs = _phase_of(lhs, bsize);
  const uint32_t prhs = _phase_of(rhs, bsize);
  const uint32_t llhs = pgas_gpa_to_rank(lhs);
  const uint32_t lrhs = pgas_gpa_to_rank(rhs);
  const uint64_t blhs = _block_of(lhs, bsize);
  const uint64_t brhs = _block_of(rhs, bsize);

  const int64_t dphase = plhs - (int64_t)prhs;
  const int32_t dlocality = llhs - (int32_t)lrhs;
  const int64_t dblock = blhs - (int64_t)brhs;

  // each difference in the phase is just one byte,
  // each difference in the locality is bsize bytes, and
  // each difference in the phase is entire cycle of bsize bytes
  const int64_t d = dblock * (int64_t) here->ranks * (int64_t) bsize + dlocality * (int64_t) bsize + dphase;

  // make sure we're not crazy
  DEBUG_IF (debug && _pgas_gpa_add_cyclic(rhs, d, bsize, false) != lhs) {
    dbg_error("difference between %"PRIu64" and %"PRIu64" computed incorrectly as %"PRId64"\n",
              lhs, rhs, d);
  }

  return d;
}


/// Implementation of address addition to be called from the public interface.
/// The debug parameter is used to stop recursion in debugging checks.
static hpx_addr_t _pgas_gpa_add_cyclic(hpx_addr_t gpa, int64_t bytes, uint32_t bsize, bool debug) {
  if (!bsize)
    return gpa + bytes;

  const uint32_t phase = (_phase_of(gpa, bsize) + bytes) % bsize;
  const uint32_t blocks = (_phase_of(gpa, bsize) + bytes) / bsize;
  const uint32_t rank = (pgas_gpa_to_rank(gpa) + blocks) % here->ranks;
  const uint32_t cycles = (pgas_gpa_to_rank(gpa) + blocks) / here->ranks;
  const uint64_t block = _block_of(gpa, bsize) + cycles;

  const hpx_addr_t addr = _triple_to_gpa(rank, block, phase, bsize);

  // sanity check
  DEBUG_IF (debug) {
    const void *lva = pgas_gpa_to_lva(addr);
    if (!heap_contains_lva(global_heap, lva)) {
      dbg_error("computed out of bounds address\n");
    }
    const int64_t diff = _pgas_gpa_sub_cyclic(addr, gpa, bsize, false);
    if (diff != bytes) {
      dbg_error("Address addition between address %"PRIu64" and offset %"PRId64" computed incorectly as %"PRIu64".  The difference is %"PRId64".\n", gpa, bytes, addr, diff);
    }
  }

  return addr;
}


int64_t pgas_gpa_sub_cyclic(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return _pgas_gpa_sub_cyclic(lhs, rhs, bsize, true);
}

hpx_addr_t pgas_gpa_add_cyclic(hpx_addr_t gpa, int64_t bytes, uint32_t bsize) {
  return _pgas_gpa_add_cyclic(gpa, bytes, bsize, true);
}
