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

#ifndef LIBHPX_NETWORK_COMPRESSED_H
#define LIBHPX_NETWORK_COMPRESSED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/attributes.h>

/// Forward declarations.
/// @{
struct config;
struct network;
/// @}


struct network *compressed_network_new (struct network *network)
  HPX_MALLOC;

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_COMPRESSED_H
