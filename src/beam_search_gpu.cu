#include "base.hpp"
#include "beam_state.hpp"
#include <cuda_device_runtime_api.h>
#include <cuda_runtime_api.h>
#include <thrust/detail/copy.h>
#include <thrust/detail/fill.inl>
#include <thrust/detail/raw_pointer_cast.h>
#include <thrust/device_vector.h>

#define CUDA_CHECK(ans) { cuda_check((ans), __FILE__, __LINE__); }
inline void cuda_check(cudaError_t code, const char *file, int line){
  if(code != cudaSuccess) {
    fprintf(stderr,"cuda: %s %s %d\n", cudaGetErrorString(code), file, line);
    exit(code);
  }
}

namespace beam_search_gpu {
  
  const u64 HASH_SIZE = 1ull<<29;
  const u64 HASH_MASK = HASH_SIZE-1;

  const u32 DEPTH_LIMIT = 1024;

  const u64 NUM_BLOCKS = 80;
  const u64 THREADS_PER_BLOCK = 512;

  const u64 NUM_THREADS = NUM_BLOCKS * THREADS_PER_BLOCK;
  const u64 TREE_SIZE_PER_THREAD = 3 << 14;

  __constant__ const u8 automaton_table[12] = {3,4,5,0,1,2,9,10,11,6,7,8};
  
  struct traverse_euler_tour_t {
    puzzle_data puzzle;
    beam_state  state;

    u32  max_score;
    u64* hash_table;
    u64* histogram;
    u32  low, high;
    
    u32 istep;
    u32 cutoff;
    f32 cutoff_keep_probability;

    u64  tour_current_total_size;
    u32* tours_current_size;
    u8*  tours_current;

    u64  tour_next_total_size;
    u32* tours_next_size;
    u8*  tours_next;

    __device__
    void run() {
      u32 idx = threadIdx.x+blockDim.x*blockIdx.x;

      u32 cutoff = this->cutoff;
      f32 cutoff_keep_probability = this->cutoff_keep_probability;
      u32 istep = this->istep;
      
      u8* tour_next = tours_next + idx * TREE_SIZE_PER_THREAD;
      u32 tour_next_size = 0;

      u64 block_size = (tour_current_total_size + NUM_THREADS - 1) / NUM_THREADS;
      u64 fr_size = idx * block_size;
      u64 to_size = min(tour_current_total_size, (idx + 1) * block_size) - 1;

      if(fr_size > to_size) {
        tours_next_size[idx] = 0;
        return;
      }

      u64 cur_size = 0;
      u32 itree = 0;
      u32 iedge = 0;

      u32 local_low = max_score, local_high = 0;

      while(cur_size + tours_current_size[itree] <= fr_size) {
        cur_size += tours_current_size[itree];
        itree += 1;
      }

      beam_state S = state;
      f32 cutoff_running = 1.0;

      u32 nstack_moves = 0;
      u8 stack_moves[DEPTH_LIMIT];
      u8 automaton_l[DEPTH_LIMIT+1];
      u8 automaton_r[DEPTH_LIMIT+1];
      automaton_l[0] = 12;
      automaton_r[0] = 12;
      
      u32 ncommit = 0;
      u8 committed[DEPTH_LIMIT];

      while(cur_size < fr_size) {
        u8* tree = tours_current + itree * TREE_SIZE_PER_THREAD;

        u8 edge = tree[iedge];
        if(edge > 0) {
          S.do_move(puzzle, edge - 1);
          if(edge <= 7) {
            automaton_l[nstack_moves+1] = automaton_table[edge - 1];
            automaton_r[nstack_moves+1] = automaton_r[nstack_moves];
          }else{
            automaton_l[nstack_moves+1] = automaton_l[nstack_moves];
            automaton_r[nstack_moves+1] = automaton_table[edge - 1];
          }
          stack_moves[nstack_moves] = edge - 1;
          nstack_moves += 1;
        }else{
          nstack_moves -= 1;
          S.undo_move(puzzle, stack_moves[nstack_moves]);
        }

        cur_size += 1;
        iedge += 1;
        while(itree < NUM_THREADS && iedge == tours_current_size[itree]) {
          itree += 1;
          iedge = 0;
        }
      }

      while(cur_size <= to_size) {

        while(cur_size <= to_size) {
          u8* tree = tours_current + itree * TREE_SIZE_PER_THREAD;

          u8 edge = tree[iedge];
          if(edge > 0) {
            S.do_move(puzzle, edge - 1);
            if(edge <= 7) {
              automaton_l[nstack_moves+1] = automaton_table[edge - 1];
              automaton_r[nstack_moves+1] = automaton_r[nstack_moves];
            }else{
              automaton_l[nstack_moves+1] = automaton_l[nstack_moves];
              automaton_r[nstack_moves+1] = automaton_table[edge - 1];
            }
            if(ncommit == nstack_moves &&
               committed[nstack_moves] == edge - 1 &&
               tour_next_size > 0) {
              stack_moves[nstack_moves] = edge - 1;
              ncommit += 1;
              nstack_moves += 1;
              tour_next_size -= 1;
            }else{
              stack_moves[nstack_moves] = edge - 1;
              nstack_moves += 1;
            }
          }else{
            if(nstack_moves == istep && fr_size <= cur_size && cur_size <= to_size) {
              break;
            }

            if(ncommit == nstack_moves) {
              tour_next[tour_next_size++] = 0;
              ncommit -= 1;
            }

            nstack_moves -= 1;
            S.undo_move(puzzle, stack_moves[nstack_moves]);
          }

          cur_size += 1;
          iedge += 1;
          while(itree < NUM_THREADS && iedge == tours_current_size[itree]) {
            itree += 1;
            iedge = 0;
          }
        }
        
        {
          if(cur_size > to_size) break;

          FOR(m, 12) if(m != automaton_l[nstack_moves] &&
                        m != automaton_r[nstack_moves]) {
            S.do_move(puzzle, m);
            u32 v = S.value(puzzle);
            bool keep = v < cutoff;
            if(v == cutoff) {
              cutoff_running += cutoff_keep_probability;
              if(cutoff_running >= 1.0) {
                cutoff_running -= 1.0;
                keep = 1;
              }
            }
            if(keep) {
              u64 h = S.get_hash(puzzle);
              u64 h_prev
                = atomicExch((unsigned long long int*)&hash_table[h&HASH_MASK],
                             h);
              if(h_prev != h) {
                while(ncommit < nstack_moves) {
                  tour_next[tour_next_size++] = 1+stack_moves[ncommit];
                  ncommit += 1;
                }
                    
                tour_next[tour_next_size++] = 1+m;
                tour_next[tour_next_size++] = 0;

                FOR(m2, 12) {
                  auto [v,h] = S.plan_move(puzzle, m2);
                  local_low = min<u32>(local_low, v);
                  local_high = max<u32>(local_high, v);
                  atomicAdd((unsigned long long int*)(histogram + v),
                            (unsigned long long int)1);
                }
              }
            }
            S.undo_move(puzzle, m);
          }
          
          if(ncommit == nstack_moves) {
            tour_next[tour_next_size++] = 0;
            ncommit -= 1;
          }

          nstack_moves -= 1;
          S.undo_move(puzzle, stack_moves[nstack_moves]);

          cur_size += 1;
          iedge += 1;
          while(itree < NUM_THREADS && iedge == tours_current_size[itree]) {
            itree += 1;
            iedge = 0;
          }
        }
      }

      while(ncommit) {
        tour_next[tour_next_size++] = 0;
        ncommit -= 1;
      }
      
      tours_next_size[idx] = tour_next_size;
     
      atomicMin(&low, local_low);
      atomicMax(&high, local_high);
      atomicAdd((unsigned long long int*)&tour_next_total_size,
                (unsigned long long int)tour_next_size);
    }
  };
    
  __global__
  __launch_bounds__(512, 2)
    void traverse_euler_tour(traverse_euler_tour_t* T) {
    T->run();
  }
  
  vector<u8> beam_search
  (puzzle_data const& puzzle,
   puzzle_state const& initial_state,
   u8 initial_direction,
   i64 width)
  {
    beam_state state; state.reset(puzzle, initial_state, initial_direction);
    i32 max_score = state.total_distance + 512;
    debug(max_score);

    traverse_euler_tour_t *traverse_data;
    CUDA_CHECK(cudaMallocManaged(&traverse_data, sizeof(traverse_euler_tour_t)));

    thrust::device_vector<u64> hash_table(HASH_SIZE, 0);    
    thrust::device_vector<u64> histogram(max_score, 0);
    
    thrust::device_vector<u32> tours_current_size(NUM_THREADS, 0);    
    thrust::device_vector<u8>  tours_current(NUM_THREADS * TREE_SIZE_PER_THREAD);
    thrust::device_vector<u32> tours_next_size(NUM_THREADS, 0);    
    thrust::device_vector<u8>  tours_next(NUM_THREADS * TREE_SIZE_PER_THREAD);

    i32 cutoff = max_score;
    f32 cutoff_keep_probability = 1.0;

    u64 tour_current_total_size = 24;
    tours_current_size[0] = 24;
    FOR(m, 12) { tours_current[2*m] = 1+m; tours_current[2*m+1] = 0; }

    CUDA_CHECK(cudaDeviceSynchronize());
    
    for(i32 istep = 1; istep < DEPTH_LIMIT; ++istep) {
      timer timer_s;

      traverse_data->puzzle = puzzle;
      traverse_data->state = state;

      traverse_data->max_score = max_score;
      traverse_data->hash_table
        = thrust::raw_pointer_cast(hash_table.data());
      traverse_data->histogram
        = thrust::raw_pointer_cast(histogram.data());
      traverse_data->low = max_score;
      traverse_data->high = 0;
    
      traverse_data->istep = istep;
      traverse_data->cutoff = cutoff;
      traverse_data->cutoff_keep_probability = cutoff_keep_probability;

      traverse_data->tour_current_total_size = tour_current_total_size;
      traverse_data->tours_current_size
        = thrust::raw_pointer_cast(tours_current_size.data());
      traverse_data->tours_current
        = thrust::raw_pointer_cast(tours_current.data());
      traverse_data->tour_next_total_size = 0;
      traverse_data->tours_next_size
        = thrust::raw_pointer_cast(tours_next_size.data());
      traverse_data->tours_next
        = thrust::raw_pointer_cast(tours_next.data());
    
      traverse_euler_tour<<<NUM_BLOCKS, THREADS_PER_BLOCK>>>
        (traverse_data);
      
      CUDA_CHECK(cudaPeekAtLastError());
      CUDA_CHECK(cudaDeviceSynchronize());

      u32 low = traverse_data->low;
      u32 high = traverse_data->high;
      u64 tour_next_total_size = traverse_data->tour_next_total_size;

      vector<i32> L_histogram(high-low+1);
      thrust::copy(begin(histogram) + low, begin(histogram) + high+1, begin(L_histogram));
      CUDA_CHECK(cudaDeviceSynchronize());
      thrust::fill(begin(histogram) + low, begin(histogram) + high+1, 0);
      CUDA_CHECK(cudaDeviceSynchronize());

      { i64 total_count = 0;
        cutoff = max_score;
        cutoff_keep_probability = 1.0;
        FORU(i, low, high) {
          if(total_count + L_histogram[i-low] > width) {
            cutoff = i;
            cutoff_keep_probability
              = (float)(width-total_count) / (float)(L_histogram[i-low]);
            break;
          }
          total_count += L_histogram[i-low];
        }
      }
    
      cerr << setw(6) << (istep+2) <<
        ": scores = " << setw(3) << low << ".." << setw(3) << cutoff << ".." << setw(3) << high <<
        ", tree size = " << setw(12) << tour_next_total_size <<
        ", elapsed = " << setw(10) << timer_s.elapsed() << "s" <<
        endl;

      if(low == 0) {
        break; // TODO
      }
      
      swap(tours_current, tours_next);
      swap(tours_current_size, tours_next_size);
      tour_current_total_size = tour_next_total_size;
    }

    return {};
  }
}
