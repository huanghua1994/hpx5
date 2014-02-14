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
#ifndef LIBHPX_BUILTINS_H
#define LIBHPX_BUILTINS_H

#define likely(S) (__builtin_expect(S, 1))
#define unlikely(S) (__builtin_expect(S, 0))
#define unreachable() __builtin_unreachable()

#endif
