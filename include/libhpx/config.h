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
#ifndef LIBHPX_CONFIG_H
#define LIBHPX_CONFIG_H

/// @file
/// @brief Types and constants needed for configuring HPX at run-time.
#include <stddef.h>
#include <stdint.h>
#include <hpx/attributes.h>

#ifdef HAVE_PHOTON
# include "config_photon.h"
#endif

#define LIBHPX_OPT_BITSET_ALL UINT64_MAX
#define LIBHPX_OPT_BITSET_NONE 0

//! Configuration options for which global memory model to use.
typedef enum {
  HPX_GAS_DEFAULT = 0, //!< Let HPX choose what memory model to use.
  HPX_GAS_SMP,         //!< Do not use global memory.
  HPX_GAS_PGAS,        //!< Use PGAS (i.e. global memory is fixed).
  HPX_GAS_AGAS,        //!< Use AGAS (i.e. global memory may move).
  HPX_GAS_PGAS_SWITCH, //!< Use hardware-accelerated PGAS.
  HPX_GAS_AGAS_SWITCH, //!< Use hardware-accelerated AGAS.
  HPX_GAS_MAX
} hpx_gas_t;

static const char* const HPX_GAS_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "PGAS",
  "AGAS",
  "PGAS_SWITCH",
  "AGAS_SWITCH",
  "INVALID_ID"
};

//! Configuration options for the network transports HPX can use.
typedef enum {
  HPX_TRANSPORT_DEFAULT = 0, //!< Let HPX choose what transport to use.
  HPX_TRANSPORT_SMP,         //!< Do not use a network transport.
  HPX_TRANSPORT_MPI,         //!< Use MPI for network transport.
  HPX_TRANSPORT_PORTALS,     //!< Use Portals for network transport.
  HPX_TRANSPORT_PHOTON,      //!< Use Photon for network transport.
  HPX_TRANSPORT_MAX
} hpx_transport_t;

static const char* const HPX_TRANSPORT_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "MPI",
  "PORTALS",
  "PHOTON",
  "INVALID_ID"
};

typedef enum {
  LIBHPX_NETWORK_DEFAULT = 0,
  LIBHPX_NETWORK_SMP,
  LIBHPX_NETWORK_PWC,
  LIBHPX_NETWORK_ISIR,
  LIBHPX_NETWORK_MAX
} libhpx_network_t;

static const char * const LIBHPX_NETWORK_TO_STRING[] = {
  "DEFAULT",
  "NONE",
  "PWC",
  "ISIR",
  "INVALID_ID"
};

//! Configuration options for which bootstrappers HPX can use.
typedef enum {
  HPX_BOOT_DEFAULT = 0,      //!< Let HPX choose what bootstrapper to use.
  HPX_BOOT_SMP,              //!< Use the SMP bootstrapper.
  HPX_BOOT_MPI,              //!< Use mpirun to bootstrap HPX.
  HPX_BOOT_PMI,              //!< Use the PMI bootstrapper.
  HPX_BOOT_MAX
} hpx_boot_t;

static const char* const HPX_BOOT_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "MPI",
  "PMI",
  "INVALID_ID"
};

//! Locality types in HPX.
#define HPX_LOCALITY_NONE  -2                   //!< Represents no locality.
#define HPX_LOCALITY_ALL   -1                   //!< Represents all localities.

//! Configuration options for runtime logging in HPX.
#define HPX_LOG_DEFAULT 0x1               //!< The default logging level.
#define HPX_LOG_BOOT    0x2               //!< Log the bootstrapper execution.
#define HPX_LOG_SCHED   0x4               //!< Log the HPX scheduler operations.
#define HPX_LOG_GAS     0x8               //!< Log the Global-Address-Space ops.
#define HPX_LOG_LCO     0x16              //!< Log the LCO operations.
#define HPX_LOG_NET     0x32              //!< Turn on logging for network ops.
#define HPX_LOG_TRANS   0x64              //!< Log the transport operations.
#define HPX_LOG_PARCEL  0x128             //!< Parcel logging.

#define HPX_TRACE_PARCELS 0x1
#define HPX_TRACE_PWC     0x2
#define HPX_TRACE_SCHED   0x4

/// The HPX configuration type.
///
/// This configuration is used to control some of the runtime
/// parameters for the HPX system.
typedef struct config {
#define LIBHPX_OPT(group, id, init, ctype) ctype group##id;
# include "options.def"
#undef LIBHPX_OPT
} config_t;

config_t *config_new(int *argc, char ***argv)
  HPX_INTERNAL HPX_MALLOC;

void config_delete(config_t *cfg)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Add declarations to query each of the set options.
///
/// @param          cfg The configuration to query.
/// @param        value The value to check for.
#define LIBHPX_OPT_INTSET(group, id, UNUSED2, UNUSED3, UNUSED4)     \
  int config_##group##id##_isset(const config_t *cfg, int value)    \
    HPX_INTERNAL HPX_NON_NULL(1);

#define LIBHPX_OPT_BITSET(group, id, UNUSED2)                       \
  static inline uint64_t                                            \
  config_##group##id##_isset(const config_t *cfg, uint64_t mask) {  \
    return (cfg->group##id & mask);                                 \
  }
# include "options.def"
#undef LIBHPX_OPT_BITSET
#undef LIBHPX_OPT_INTSET

#endif // LIBHPX_CONFIG_H
