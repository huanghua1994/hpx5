//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef PXGL_GTEPS_H
#define PXGL_GTEPS_H

#include "hpx/hpx.h"
#include "adjacency_list.h"

// Compute GTEPS
extern hpx_action_t gteps_calculate;
extern int gteps_calculate_action(const uint64_t *const g);

#endif // PXGL_GTEPS_H
