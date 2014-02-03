/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/padding.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#ifndef LIBHPX_PARCEL_PADDING_H_
#define LIBHPX_PARCEL_PADDING_H_

/**
 * Given a number of bytes, how many bytes of padding do we need to get a size
 * that is a multiple of HPX_CACHELINE_SIZE? Macro because it's used in
 * structure definitions for padding.
 */
#define PAD_TO_CACHELINE(N_)                                            \
  ((HPX_CACHELINE_SIZE - (N_ % HPX_CACHELINE_SIZE)) % HPX_CACHELINE_SIZE)

#endif /* LIBHPX_PARCEL_PADDING_H_ */
