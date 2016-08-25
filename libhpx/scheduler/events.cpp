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

#include "libhpx/Worker.h"
#include "libhpx/events.h"
#include "hpx/hpx.h"

/// Thread tracing events.
/// @{
void
Worker::EVENT_THREAD_RUN(hpx_parcel_t *p) {
  if (p == system_) {
    return;
  }
#ifdef HAVE_APEX
  // if this is NOT a null or lightweight action, send a "start" event to APEX
  if (p->action != hpx_lco_set_action) {
    CHECK_ACTION(p->action);
    void* handler = (void*)actions[p->action].handler;
    profiler_ = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  EVENT_PARCEL_RUN(p->id, p->action, p->size);
}

void
Worker::EVENT_THREAD_END(hpx_parcel_t *p) {
  if (p == system_) {
    return;
  }
#ifdef HAVE_APEX
  if (profiler != NULL) {
    apex_stop((apex_profiler_handle)(profiler));
    profiler = NULL;
  }
#endif
  EVENT_PARCEL_END(p->id, p->action);
}

void
Worker::EVENT_THREAD_SUSPEND(hpx_parcel_t *p) {
  if (p == system_) {
    return;
  }
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(profiler));
    profiler = NULL;
  }
#endif
  EVENT_PARCEL_SUSPEND(p->id, p->action);
}

void
Worker::EVENT_THREAD_RESUME(hpx_parcel_t *p) {
  if (p == system_) {
    return;
  }
#ifdef HAVE_APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)actions[p->action].handler;
    profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  EVENT_PARCEL_RESUME(p->id, p->action);
}
/// @}

