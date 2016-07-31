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

#include "none.h"

using libhpx::gas::None;

None::None()
{
}

None::~None()
{
}

void
None::set(hpx_addr_t gva, int worker)
{
}

void
None::clear(hpx_addr_t gva)
{
}

int
None::get(hpx_addr_t gva) const
{
  return -1;
}