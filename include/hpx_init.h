
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library initialization and cleanup function definitions
  hpx_init.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#pragma once
#ifndef LIBHPX_INIT_H_
#define LIBHPX_INIT_H_

#include "hpx_error.h"


/*
 --------------------------------------------------------------------
  Library-wide Definitions
 --------------------------------------------------------------------
*/

/* make ucontext functions available */
#define _XOPEN_SOURCE                                              1
#define _BSD_SOURCE                                                1


/*
 --------------------------------------------------------------------
  Initialization & Cleanup Functions
 --------------------------------------------------------------------
*/

hpx_error_t hpx_init(void);
void hpx_cleanup(void);

#endif


