// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_MEMORY_H
#define LIBHPX_MEMORY_H

/// @file include/libhpx/hpx.h
/// @brief Address spaces.
///
/// This header defines the interface to the various kinds of memory that we
/// allocate. In addition to standard local memory, we can allocate network
/// registered memory, global memory, and global cyclic memory.

enum {
  AS_REGISTERED = 0,
  AS_GLOBAL,
  AS_CYCLIC,
  AS_COUNT
};

#ifndef HAVE_NETWORK

/// If we don't have a network configured, then we don't do anything interesting
/// with memory. Our interface just forwards inline to the stdlib
/// allocators. This works fine, even if the user has configured
/// --enable-jemalloc, since jemalloc is handling the stdlib allocation in that
/// context.

# include <stdlib.h>

static inline void as_join(int id) {
}

static inline void as_leave(void) {
}

static inline void as_thread_init(void) {
}

static inline void *as_malloc(int id, size_t bytes) {
  return malloc(bytes);
}

static inline void *as_calloc(int id, size_t nmemb, size_t bytes) {
  return calloc(nmemb, bytes);
}

static inline void *as_memalign(int id, size_t boundary, size_t size) {
  void *ptr = NULL;
  posix_memalign(&ptr, boundary, size);
  return ptr;
}

static inline void as_free(int id, void *ptr) {
  free(ptr);
}

#elif defined(HAVE_JEMALLOC)

/// When a network has been configured then we actually need to do something
/// about these allocations. In this context we are guaranteed that the jemalloc
/// header is available, so we can use its types here.
///
/// The JEMALLOC_NO_DEMANGLE flag exposes jemalloc's je_-prefixed symbols so
/// that we can use them here. These are necessary because we prefix the
/// jemalloc symbols with --disable-jemalloc, so we don't inadvertently use
/// jemalloc's malloc/free functions if the user hasn't asked us to. The je_
/// prefixes deal with this, and will always "point" to the symbols that
/// jemalloc is actually exposing.
#define JEMALLOC_NO_DEMANGLE
#include <jemalloc/jemalloc.h>

/// A chunk allocator.
///
/// The chunk allocator parameterizes an address space, and provides jemalloc
/// with the callbacks necessary to get more memory to manage. The default
/// jemalloc allocator uses mmap (decorated to provide aligned allocations),
/// munmap, and madvise for it's allocator.
///
/// In HPX, we want to do things like register the chunks or get them from a
/// specific address range, or fragment up huge TLB pages, or all three. At
/// initialization time the system will set an allocator for each address space
/// that needs custom handling. *This must be done before system threads join
/// the address space with as_join().*
///
/// @{
typedef struct {
  chunk_alloc_t *challoc;
  chunk_dalloc_t *chfree;
  chunk_purge_t *chpurge;
} chunk_allocator_t;
/// @}

/// Each thread "joins" the custom address space world by figuring out what
/// flags to pass to jemalloc for each address space, and storing them in this
/// array.
/// @{
extern __thread int as_flags[AS_COUNT];
/// @}

/// Set the allocator for an address space.
///
/// The address space does *not* take ownership of the allocator, and does not
/// try to free it at termination.
///
/// The default allocator is the local allocator, if no custom allocator is set
/// for an address space then it will use the local address space to satisfy
/// allocations, e.g., without a custom allocator as_malloc(ID, n) forwards to
/// malloc(n).
///
/// @param              id The address space id to update.
/// @param       allocator An allocator implementation.
void as_set_allocator(int id, chunk_allocator_t *allocator);

/// Call by each thread to join the memory system.
///
/// After calling as_join(), threads can use the as_* allocation interface. This
/// must be called *after* any custom allocator has been installed.
void as_join(int id);

/// Called by each thread to leave the memory system.
///
/// This will flush any caches that have been set. It does not to anything to
/// the backing arenas since we don't know where that memory is being
/// used. Jemalloc may purge those arenas to reclaim the backing memory, and
/// they will be cleaned up at shutdown.
///
/// The main consequence of not freeing the arenas is that global address
/// regions can not be returned to the global bitmap for use
/// elsewhere. Essentially the arena will hold onto its chunks until the end of
/// time.
void as_leave(void);

/// Get the number of bytes associated with a "chunk" of memory, in an
/// allocator-independent way.
size_t as_bytes_per_chunk(void);

static inline void *as_malloc(int id, size_t bytes) {
  return je_mallocx(bytes, as_flags[id]);
}

static inline void *as_calloc(int id, size_t nmemb, size_t bytes) {
  int flags = as_flags[id] | MALLOCX_ZERO;
  return je_mallocx(nmemb * bytes, flags);
}

static inline void *as_memalign(int id, size_t boundary, size_t size) {
  int flags = as_flags[id] | MALLOCX_ALIGN(boundary);
  return je_mallocx(size, flags);
}

static inline void as_free(int id, void *ptr)  {
  je_dallocx(ptr, as_flags[id]);
}

#undef JEMALLOC_NO_DEMANGLE

#elif defined(HAVE_TBBMALLOC)

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

extern void* pools[AS_COUNT];

void as_join(int id);
void as_leave(void);
size_t as_bytes_per_chunk(void);

void *as_malloc(int id, size_t bytes);
void *as_calloc(int id, size_t nmemb, size_t bytes);
void *as_memalign(int id, size_t boundary, size_t size);
void as_free(int id, void *ptr);

#ifdef __cplusplus
}
#endif

#else
# error No gas allocator configured
#endif

#ifdef ENBALE_INSTRUMENTATION
static inline void TRACE_REGISTERED_MALLOC(void *ptr, size_t n, size_t align) {
  static const int class = HPX_INST_CLASS_MEMORY;
  static const int id = HPX_INST_EVENT_MEMORY_REGISTERED_ALLOC;
  inst_trace(class, id, ptr, n, align);
}

static inline void TRACE_REGISTERED_FREE(void *ptr) {
  static const int class = HPX_INST_CLASS_MEMORY;
  static const int id = HPX_INST_EVENT_MEMORY_REGISTERED_FREE;
  inst_trace(class, id, ptr);
}

static inline void TRACE_GLOBAL_MALLOC(void *ptr, size_t n, size_t align) {
  static const int class = HPX_INST_CLASS_MEMORY;
  static const int id = HPX_INST_EVENT_MEMORY_GLOBAL_ALLOC;
  inst_trace(class, id, ptr, n, align);
}

static inline void TRACE_GLOBAL_FREE(void *ptr) {
  static const int class = HPX_INST_CLASS_MEMORY;
  static const int id = HPX_INST_EVENT_MEMORY_GLOBAL_FREE;
  inst_trace(class, id, ptr);
}

static inline void TRACE_CYCLIC_MALLOC(void *ptr, size_t n, size_t align) {
  static const int class = HPX_INST_CLASS_MEMORY;
  static const int id = HPX_INST_EVENT_MEMORY_CYCLIC_ALLOC;
  inst_trace(class, id, ptr, n, align);
}

static inline void TRACE_CYCLIC_FREE(void *ptr) {
  static const int class = HPX_INST_CLASS_MEMORY;
  static const int id = HPX_INST_EVENT_MEMORY_CYCLIC_FREE;
  inst_trace(class, id, ptr);
}
#else
# define TRACE_MALLOC(ptr, n, align)
# define TRACE_FREE(ptr)
# define TRACE_REGISTERED_MALLOC(ptr, n, align)
# define TRACE_REGISTERED_FREE(ptr)
# define TRACE_GLOBAL_MALLOC(ptr, n, align)
# define TRACE_GLOBAL_FREE(ptr)
# define TRACE_CYCLIC_MALLOC(ptr, n, align)
# define TRACE_CYCLIC_FREE(ptr)
#endif

static inline void *registered_malloc(size_t bytes) {
  void *ptr = as_malloc(AS_REGISTERED, bytes);
  TRACE_REGISTERED_MALLOC(ptr, bytes, 0);
  return ptr;
}

static inline void *registered_calloc(size_t nmemb, size_t bytes) {
  void *ptr = as_calloc(AS_REGISTERED, nmemb, bytes);
  TRACE_REGISTERED_MALLOC(ptr, nmemb * bytes, 0);
  return ptr;
}

static inline void *registered_memalign(size_t boundary, size_t size) {
  void *ptr = as_memalign(AS_REGISTERED, boundary, size);
  TRACE_REGISTERED_MALLOC(ptr, size, boundary);
  return ptr;
}

static inline void registered_free(void *ptr)  {
  TRACE_REGISTERED_FREE(ptr);
  as_free(AS_REGISTERED, ptr);
}

static inline void *global_malloc(size_t bytes) {
  void *ptr = as_malloc(AS_GLOBAL, bytes);
  TRACE_GLOBAL_MALLOC(ptr, bytes, 0);
  return ptr;
}

static inline void *global_calloc(size_t nmemb, size_t bytes) {
  void *ptr = as_calloc(AS_GLOBAL, nmemb, bytes);
  TRACE_GLOBAL_MALLOC(ptr, nmemb * bytes, 0);
  return ptr;
}

static inline void *global_memalign(size_t boundary, size_t size) {
  void *ptr = as_memalign(AS_GLOBAL, boundary, size);
  TRACE_GLOBAL_MALLOC(ptr, size, boundary);
  return ptr;
}

static inline void global_free(void *ptr)  {
  TRACE_GLOBAL_FREE(ptr);
  as_free(AS_GLOBAL, ptr);
}

static inline void *cyclic_malloc(size_t bytes) {
  void *ptr = as_malloc(AS_CYCLIC, bytes);
  TRACE_CYCLIC_MALLOC(ptr, bytes, 0);
  return ptr;
}

static inline void *cyclic_calloc(size_t nmemb, size_t bytes) {
  void *ptr = as_calloc(AS_CYCLIC, nmemb, bytes);
  TRACE_CYCLIC_MALLOC(ptr, nmemb * bytes, 0);
  return ptr;
}

static inline void *cyclic_memalign(size_t boundary, size_t size) {
  void *ptr = as_memalign(AS_CYCLIC, boundary, size);
  TRACE_CYCLIC_MALLOC(ptr, size, boundary);
  return ptr;
}

static inline void cyclic_free(void *ptr)  {
  TRACE_CYCLIC_FREE(ptr);
  as_free(AS_CYCLIC, ptr);
}

#endif // LIBHPX_MEMORY_H
