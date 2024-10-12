#include "beam_state.hpp"
#include <cuda_device_runtime_api.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>
#include <thrust/detail/raw_pointer_cast.h>
#include <thrust/device_ptr.h>
#include <thrust/system_error.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/execution_policy.h>

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line){
  if(code != cudaSuccess) {
    fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
    exit(code);
  }
}

namespace beam_search_gpu {

  const i64 TREE_SIZE = 1<<14;
  
  const u64 HASH_SIZE = 1ull<<26;
  const u64 HASH_MASK = HASH_SIZE-1;

  using euler_tour_edge = u8;
  struct euler_tour {
    i64 max_size;
    i64 size;
    euler_tour_edge* data;
  
    FORCE_INLINE void reset() { size = 0; }
    FORCE_INLINE void push(i32 x) {
      data[size++] = x;
    }
    FORCE_INLINE u8& operator[](i32 ix) { return data[ix]; }
  };

  struct traverse_input {
    puzzle_data P;
    puzzle_state initial_state;
    u8 initial_direction;
    i32 num_tours_current;
    i32 istep;
    i32 cutoff;
    f32 cutoff_keep_probability;
  };
  
  struct traverse_output {
    i64 total_size;
    i32 next_tour_current;
    i32 num_tours_next;
    i64 low;
    i64 high;
  };

  __global__
  void traverse_euler_tour
  (traverse_input const* I,
   traverse_output* O,
   RNG* rngs,
   u64* hash_table,
   i32* tours_current_size, u8* tours_current,
   i32* tours_next_size, u8* tours_next,
   i64* histogram)
  {   
    i32* tour_next_size = 0;
    u8* tour_next = 0;

    int idx = threadIdx.x+blockDim.x*blockIdx.x;
    RNG& rng = rngs[idx];
    
    while(1) {
      i32 ix = atomicAdd(&O->next_tour_current, 1);
      if(ix >= I->num_tours_current) break;
      
      beam_state S; S.reset(I->P, I->initial_state, I->initial_direction);
      
      i32 size = tours_current_size[ix];
      u8* tour_current = tours_current + ix * TREE_SIZE;

      i32 nstack_moves = 0;
      u8 stack_moves[MAX_SOLUTION_SIZE];
      // u32 stack_automaton[MAX_SOLUTION_SIZE];
      // stack_automaton[0] = 6*6+6;

      i32 ncommit = 0;
    
      FOR(iedge, size) {
        auto const& edge = tour_current[iedge];
        if(edge > 0) {
          stack_moves[nstack_moves] = edge - 1;
          S.do_move(I->P, stack_moves[nstack_moves]);
          // stack_automaton[nstack_moves+1]
          //   = automaton::next_state[stack_automaton[nstack_moves]][stack_moves[nstack_moves]];
          nstack_moves += 1;
        }else{
          if(nstack_moves == I->istep) {
            auto v = S.value(I->P);
            if(v < I->cutoff ||
               (v == I->cutoff && rng.randomFloat() < I->cutoff_keep_probability)) {
              if(!tour_next) {
                i32 ix2 = atomicAdd(&O->num_tours_next, 1);
                tour_next_size = tours_next_size + ix2;
                tour_next = tours_next + ix2 * TREE_SIZE;
                *tour_next_size = 0;
              }
              while(ncommit < nstack_moves) {
                tour_next[(*tour_next_size)++] = 1+stack_moves[ncommit];
                ncommit += 1;
              }

              FOR(m, 12)
                // if(automaton::allow_move[stack_automaton[istep]]&bit(m))
                {
                  auto [v,h] = S.plan_move(I->P, m);
                  auto prev
                    = atomicExch((unsigned long long int*)&hash_table[h&HASH_MASK],
                                 h);
                  if(prev != h) {
                    atomicMin((long long int*)&O->low, (long long int)v);
                    atomicMax((long long int*)&O->high, (long long int)v);
                    atomicAdd((unsigned long long int*)(histogram + v),
                              (unsigned long long int)1);
                    tour_next[(*tour_next_size)++] = 1+m;
                    tour_next[(*tour_next_size)++] = 0;
                  }
                }
            }
          }

          if(nstack_moves == 0) {
            break;
          }

          if(ncommit == nstack_moves) {
            tour_next[(*tour_next_size)++] = 0;
            ncommit -= 1;
          }

          nstack_moves -= 1;
          S.undo_move(I->P, stack_moves[nstack_moves]);
        }

        if(tour_next_size != 0 && *tour_next_size + 2 * I->istep + 128 > TREE_SIZE) {
          FORD(i,ncommit-1,0) tour_next[(*tour_next_size)++] = 0;
          tour_next[(*tour_next_size)++] = 0;

          atomicAdd((unsigned long long int*)&O->total_size,
                    (unsigned long long int)(*tour_next_size));
          tour_next_size = 0;
          tour_next = 0;
          ncommit = 0;
        }
      }
    }
      
    if(tour_next_size != 0) {
      atomicAdd((unsigned long long int*)&O->total_size,
                (unsigned long long int)(*tour_next_size));
    }
  }

  vector<u8> beam_search
  (puzzle_data const& P,
   puzzle_state const& initial_state,
   u8 initial_direction,
   i64 width)
  {
    //     if(!HS) {
    //       auto ptr = new uint64_t[HASH_SIZE];
    //       HS = ptr;
    //     }

    beam_state S; S.reset(P, initial_state, initial_direction);
    i32 max_score = S.total_distance + 1000; // TODO
    debug(max_score);

    i32 num_trees = (1<<30) / TREE_SIZE;

    i32 num_tours_current = 1;
    thrust::device_vector<u64> hash_table(HASH_SIZE);
    thrust::device_vector<i32> tours_current_size(num_trees);
    thrust::device_vector<u8>  tours_current(num_trees * TREE_SIZE);
    thrust::device_vector<i32> tours_next_size(num_trees);
    thrust::device_vector<u8>  tours_next(num_trees * TREE_SIZE);
    tours_current_size[0] = 1;
    tours_current[0] = 0;
    
    traverse_input* device_I;
    gpuErrchk(cudaMallocManaged((void**)&device_I, sizeof(traverse_input)));
    traverse_output* device_O;
    gpuErrchk(cudaMallocManaged((void**)&device_O, sizeof(traverse_output)));

    gpuErrchk(cudaDeviceSynchronize());

    device_I->P = P;
    device_I->initial_state = initial_state;
    device_I->initial_direction = initial_direction;

    i32 cutoff = max_score;
    f32 cutoff_keep_probability = 1.0;

    i32 num_blocks  = 2048;
    i32 num_threads = 128;

    thrust::device_vector<RNG> rngs(num_blocks * num_threads);
    auto seed = time(0);
    FOR(i, num_blocks * num_threads) {
      RNG t; t.reset(seed + i);
      rngs[i] = t;
    }
    
    for(i32 istep = 0; istep < 50; ++istep) {
      timer timer_s;

      thrust::device_vector<i64> histogram(max_score+1, 0);

      device_I->num_tours_current = num_tours_current;
      device_I->istep = istep;
      device_I->cutoff = cutoff;
      device_I->cutoff_keep_probability = cutoff_keep_probability;
      device_O->total_size = 0;
      device_O->next_tour_current = 0;
      device_O->num_tours_next = 0;
      device_O->low = max_score;
      device_O->high = 0;

      gpuErrchk(cudaDeviceSynchronize());
      
      traverse_euler_tour<<<num_blocks, num_threads>>>
        (device_I, device_O,
         thrust::raw_pointer_cast(rngs.data()),
         thrust::raw_pointer_cast(hash_table.data()),
         thrust::raw_pointer_cast(tours_current_size.data()),
         thrust::raw_pointer_cast(tours_current.data()),
         thrust::raw_pointer_cast(tours_next_size.data()),
         thrust::raw_pointer_cast(tours_next.data()),
         thrust::raw_pointer_cast(histogram.data())
         );
      gpuErrchk(cudaPeekAtLastError());

      gpuErrchk(cudaDeviceSynchronize());


      thrust::host_vector<i64> hist(histogram);

      { i64 total_count = 0;
        cutoff = max_score;
        cutoff_keep_probability = 1.0;
        FORU(i, device_O->low, device_O->high) {
          if(total_count+histogram[i] > width) {
            cutoff = i;
            cutoff_keep_probability = (float)(width-total_count) / (float)(hist[i]);
            break;
          }
          total_count += histogram[i];
        }
      }
      
      cerr << setw(6) << (istep+1) <<
        ": scores = " << setw(3) << device_O->low << ".." << setw(3) << cutoff <<
        ", tree size = " << setw(12) << device_O->total_size <<
        ", num trees = " << setw(4) << device_O->num_tours_next <<
        ", elapsed = " << setw(10) << timer_s.elapsed() << "s" <<
        endl;

      swap(tours_current, tours_next);
      swap(tours_current_size, tours_next_size);
      num_tours_current = device_O->num_tours_next;
    }

    return {};
  }

}
