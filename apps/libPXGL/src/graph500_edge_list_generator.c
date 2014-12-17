#define __STDC_FORMAT_MACROS

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <math.h>

#include "generatorhelper/include/splittable_mrg.h" //for mrg_state

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "pxgl/edge_list.h"

#include "libpxgl/graph_generator.h"

extern void make_one_edge(int64_t nverts, int level, int lgN, mrg_state* st, const graph500_edge_list_local_args_t * const args, uint64_t val0, uint64_t val1,int64_t start_edge,int64_t* count);


int power(int base, int exponent) {
  int i, result = 1;
  for(i = 1 ; i<=exponent; i++) {
    result = result * base;
  }
  return result;
}


void make_mrg_seed(uint64_t userseed1, uint64_t userseed2, uint_fast32_t* seed) {
  seed[0] = (uint32_t)(userseed1 & UINT32_C(0x3FFFFFFF)) + 1;
  seed[1] = (uint32_t)((userseed1 >> 30) & UINT32_C(0x3FFFFFFF)) + 1;
  seed[2] = (uint32_t)(userseed2 & UINT32_C(0x3FFFFFFF)) + 1;
  seed[3] = (uint32_t)((userseed2 >> 30) & UINT32_C(0x3FFFFFFF)) + 1;
  seed[4] = (uint32_t)((userseed2 >> 60) << 4) + (uint32_t)(userseed1 >> 60) + 1;
}


/* Generate a range of edges (from start_edge to end_edge of the total graph),  writing into elements [0, end_edge - start_edge) of the edges array.  This  code is parallel on OpenMP and XMT; it must be used with separately-implemented SPMD parallelism for MPI. */
void generate_kronecker_range(
			      const uint_fast32_t seed[5] /* All values in [0, 2^31 - 1), not all zero */,
			      int logN /* In base 2 */,
			      int64_t start_edge, int64_t end_edge,
			       const graph500_edge_list_local_args_t * const args) {
  mrg_state state;
  int64_t nverts = (int64_t)1 << logN;
  int64_t ei;
  
  mrg_seed(&state, seed);

  uint64_t val0, val1; /* Values for scrambling */
  {
    mrg_state new_state = state;
    mrg_skip(&new_state, 50, 7, 0);
    val0 = mrg_get_uint_orig(&new_state);
    val0 *= UINT64_C(0xFFFFFFFF);
    val0 += mrg_get_uint_orig(&new_state);
    val1 = mrg_get_uint_orig(&new_state);
    val1 *= UINT64_C(0xFFFFFFFF);
    val1 += mrg_get_uint_orig(&new_state);
  }
  
  //printf("Inside generate kronecker range function\n");
  //printf("start edge index: %" PRId64 " end edge index %" PRId64 "\n", start_edge, end_edge);
  int64_t count = 0;
  for (ei = start_edge; ei < end_edge; ei+=2) {
    mrg_state new_state = state;
    mrg_skip(&new_state, 0, (uint64_t)ei, 0);
    //printf("Calling make one edge from kronecker\n");
    make_one_edge(nverts, 0, logN, &new_state, args/*edges + (ei - start_edge)*/, val0, val1, start_edge, &count);
    count= count+2; //bidirectional edges
  }
}

/*static void compute_edge_range(int rank, int size, int64_t M, int64_t* start_idx, int64_t* end_idx) {
  int64_t rankc = (int64_t)(rank);
  int64_t sizec = (int64_t)(size);
  *start_idx = rankc * (M / sizec) + (rankc < (M % sizec) ? rankc : (M % sizec));
  *end_idx = (rankc + 1) * (M / sizec) + (rankc + 1 < (M % sizec) ? rankc + 1 : (M % sizec));
}
*/

hpx_action_t generate_edgelist = 0;
int generate_edgelist_action(const graph500_edge_list_local_args_t * const args) {

  /* Spread the two 64-bit numbers into five nonzero values in the correct range. */
  uint_fast32_t seed[5];
  make_mrg_seed(args->userseed1, args->userseed2, seed);

  //printf("Inside generate edgelist action\n");
  int64_t start_idx = args->edges_skip;
  int64_t end_idx = args->edges_skip + args->edges_no;
  //compute_edge_range(rank, size, args->M, &start_idx, &end_idx);
  //int64_t nedges = end_idx - start_idx;

  //packed_edge* local_edges = (packed_edge*)xmalloc((size_t)nedges * sizeof(packed_edge));
 
  generate_kronecker_range(seed, args->log_numverts, start_idx, end_idx, args);
  return HPX_SUCCESS;
}

hpx_action_t graph500_edge_list_generator = 0;
int graph500_edge_list_generator_action(const graph500_edge_list_generator_args_t * const args) {
  edge_list_t *el = malloc(sizeof(*el));
  assert(el);
  hpx_addr_t edges_sync = HPX_NULL;
  printf("Starting Graph500 graph generator\n");
  hpx_time_t now = hpx_time_now();
  int log_numverts = args->scale; /* In base 2 */
  sssp_uint_t nvertices = power(2, log_numverts) ;
  el->num_vertices = nvertices;
  el->num_edges = (nvertices * args->edgefactor * 2) ; //TODO: verify, divide by 2 for bidirectional

  //TODO: needed following line?
  //el->num_vertices++;

  // Set an appropriate block size
  el->edge_list_bsize = ((el->num_edges + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(edge_list_edge_t);
  // Allocate an edge_list array in the global address space
  el->edge_list = hpx_gas_global_alloc(HPX_LOCALITIES, el->edge_list_bsize);
  // Create synchronization lco
  edges_sync = hpx_lco_and_new(el->num_edges);
  assert(edges_sync != HPX_NULL);
      
  //printf("Inside the graph500_edge_list_generator_action\n");
  
  const size_t local_args_size = sizeof( graph500_edge_list_local_args_t);
  graph500_edge_list_local_args_t *local_args = malloc(local_args_size);
  local_args->el = *el;
  local_args->edges_sync = edges_sync;
  local_args->log_numverts = log_numverts;
  local_args->M = INT64_C(16) << log_numverts;//TODO: Is M total # edges?
  local_args->userseed1 = 1;
  local_args->userseed2 = 2;
  local_args->locality_readers = args->locality_readers;
  local_args->thread_readers = args-> thread_readers;

  const int64_t thread_chunk = el->num_edges / (args->thread_readers * args->locality_readers) ;
  local_args->edges_no = thread_chunk;
  for(unsigned int locality_desc = 0; locality_desc < args->locality_readers; ++locality_desc) {
    for(unsigned int thread_desc = 0; thread_desc < args->thread_readers; ++thread_desc) {
      local_args->edges_skip = ((locality_desc * args->thread_readers) + thread_desc) * thread_chunk;
      //printf("Calling generate edgelist action\n");
      hpx_call(HPX_THERE(locality_desc), generate_edgelist, local_args, local_args_size, HPX_NULL);
    }
  }
  double elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Waiting for completion LCO.  Time took to start local read loops: %fs\n", elapsed);
  now = hpx_time_now();
  hpx_lco_wait(edges_sync);
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Finished waiting for edge list completion.  Time waiting: %fs\n", elapsed);
  hpx_lco_delete(edges_sync, HPX_NULL);

  hpx_thread_continue_cleanup(sizeof(*el), el, free, el);
  return HPX_SUCCESS;
}


static __attribute__((constructor)) void graph500_edge_list_register_actions() {
  HPX_REGISTER_ACTION(&graph500_edge_list_generator,
                      graph500_edge_list_generator_action);
  HPX_REGISTER_ACTION(&generate_edgelist,generate_edgelist_action);
}
