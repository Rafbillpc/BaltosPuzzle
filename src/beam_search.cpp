#include "beam_state.hpp"
#include "beam_search.hpp"
#include <omp.h>

const i64 MIN_TREE_SIZE = 1<<17;
i64 tree_size = MIN_TREE_SIZE;

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
  
vector<euler_tour> tree_pool;
euler_tour get_new_tree(){
  euler_tour tour;
#pragma omp critical
  {
    if(tree_pool.empty()) {
      tour.max_size = tree_size;
      tour.size = 0;
      tour.data = new u8[tree_size];
    }else{
      tour = tree_pool.back();
      tour.size = 0;
      tree_pool.pop_back();
    }
  }
  return tour;
}

const u64 HASH_SIZE = 1ull<<30;
const u64 HASH_MASK = HASH_SIZE-1;
uint64_t *HS = nullptr;

vector<u8> find_solution
(u32 goal,
 puzzle_data const& P,
 puzzle_state const& initial_state, u8 initial_direction,
 i32 istep,
 euler_tour tour_current)
{
  beam_state S; S.reset(P, initial_state, initial_direction);

  u8 stack_moves[MAX_SOLUTION_SIZE];
  i32 nstack_moves = 0;

  FOR(iedge, tour_current.size) {
    u8 edge = tour_current[iedge];
    if(edge > 0) {
      stack_moves[nstack_moves] = edge - 1;
      S.do_move(P, stack_moves[nstack_moves]);
      nstack_moves += 1;
    }else{
      if(nstack_moves == istep+1) {
        auto v = S.value(P);
        if(v == goal) {
          return vector<u8>(stack_moves, stack_moves + nstack_moves);
        }
      }

      if(nstack_moves == 0) {
        return {};
      }
				
      nstack_moves -= 1;
      S.undo_move(P, stack_moves[nstack_moves]);
    }
  }

  runtime_assert(false);
}

void traverse_euler_tour
(puzzle_data const& P,
 puzzle_state const& initial_state, u8 initial_direction,
 u32 istep,
 euler_tour tour_current,
 vector<euler_tour> &tours_next,
 u64* histogram, u32& out_low, u32& out_high,
 u32 cutoff, f32 cutoff_keep_probability,
 bool save_states, f32 save_states_probability, vector<beam_state>& saved_states)
{
  beam_state S; S.reset(P, initial_state, initial_direction);

  u8 stack_moves[MAX_SOLUTION_SIZE];
  u32 nstack_moves = 0;
  
  u32 stack_automaton[MAX_SOLUTION_SIZE];
  stack_automaton[0] = 6*6+6;

  u32 ncommit = 0;
  if(tours_next.empty()) tours_next.eb(get_new_tree());
  auto *tour_next = &tours_next.back(); 
  runtime_assert(tour_next->max_size == tree_size);

  f32 save_states_running = 1.0;
  f32 cutoff_running = 1.0;

  u32 low = out_low, high = out_high;
  
  FOR(iedge, tour_current.size) {
    u8 edge = tour_current[iedge];
    if(edge > 0) {
      stack_moves[nstack_moves] = edge-1;
      S.do_move(P, edge-1);
      stack_automaton[nstack_moves+1]
        = automaton::next_state[stack_automaton[nstack_moves]][edge-1];
      nstack_moves += 1;
    }else{
      if(nstack_moves == istep) {
        auto v = S.value(P);
        bool keep = v < cutoff;
        if(v == cutoff) {
          cutoff_running += cutoff_keep_probability;
          if(cutoff_running >= 1.0) {
            keep = 1;
            cutoff_running -= 1.0;
          }
        }
        if(keep) {
          if(save_states) {
            save_states_running += save_states_probability;
            if(save_states_running > 1.0) {
              saved_states.pb(S);
              save_states_running -= 1.0;
            }
          }
          
          while(ncommit < nstack_moves) {
            tour_next->push(1+stack_moves[ncommit]);
            ncommit += 1;
          }

          bool ks[12];
          u32  vs[12];
          
          FOR(m, 12) {
            ks[m] = 0;
            if(automaton::allow_move[stack_automaton[istep]] & bit(m)) {
              auto [v,h] = S.plan_move(P, m);
              if(v <= cutoff) {
                auto prev = HS[h&HASH_MASK];
                if(prev != h) {
                  HS[h&HASH_MASK] = h;
                  ks[m] = 1;
                  vs[m] = v;
                }
              }
            }
          }

          FOR(m, 12) if(ks[m]) {
            low = min(low, vs[m]);
            high = max(high, vs[m]);
            histogram[vs[m]] += 1;
            tour_next->push(1+m);
            tour_next->push(0);
          }
          
        }
      }

      if(nstack_moves == 0) {
        break;
      }

      if(ncommit == nstack_moves) {
        tour_next->push(0);
        ncommit -= 1;
      }
				
      nstack_moves -= 1;
      S.undo_move(P, stack_moves[nstack_moves]);
    }
    
    if(__builtin_expect(tour_next->size + 2 * istep + 128 > tree_size, false)) {
      FORD(i,ncommit-1,0) tour_next->push(0);
      tour_next->push(0);
      tours_next.eb(get_new_tree());
      tour_next = &tours_next.back();
      ncommit = 0;
    }
  }

  out_low = low;
  out_high = high;
}

beam_search_result beam_search
(puzzle_data const& P,
 puzzle_state const& initial_state,
 u8 initial_direction,
 beam_search_config config)
{
  if(!HS) {
    auto ptr = new uint64_t[HASH_SIZE];
    HS = ptr;
  }

  beam_state S; S.reset(P, initial_state, initial_direction);
  i32 max_score = S.value(P) * 2 + 1000;
  debug(max_score);
  vector<u64> histogram(max_score+1, 0);
  
  vector<euler_tour> tours_current;
  tours_current.eb(get_new_tree());
  tours_current.back().push(0);
	
  i32   cutoff = max_score;
  f32 cutoff_keep_probability = 1.0;

  i32 num_threads = omp_get_max_threads();
  debug(num_threads);

  vector<vector<u64>> L_histograms(num_threads, vector<u64>(max_score+1, 0));
  vector<vector<beam_state>> L_saved_states(num_threads);

  for(i32 istep = 0;; ++istep) {
    timer timer_s;
    vector<euler_tour> tours_next;

    bool increased_tree_size = false;
    if(tours_current.size() > 256) {
      for(auto &t : tree_pool) delete[] t.data;
      tree_pool.clear();
      tree_size *= 2;
      increased_tree_size = true;
    }

    sort(all(tours_current), [&](auto const& t1, auto const& t2) {
      return t1.size < t2.size;
    });

    u32 low = max_score, high = 0;
    
#pragma omp parallel
    {
      auto thread_id = omp_get_thread_num();
      vector<u64>& L_histogram = L_histograms[thread_id];
      vector<euler_tour> L_tours_next;
      
      u32 L_low = max_score, L_high = 0;
      
      while(1) {
        euler_tour tour_current;
#pragma omp critical
        { if(!tours_current.empty()) {
            tour_current = tours_current.back();
            tours_current.pop_back();
          }else{
            tour_current.size = 0;
          }
        }
        if(tour_current.size == 0) break;

        traverse_euler_tour
          (P, initial_state, initial_direction,
           istep,
           tour_current, 
           L_tours_next, 
           L_histogram.data(), L_low, L_high,
           cutoff, cutoff_keep_probability,
           config.save_states, config.save_states_probability, L_saved_states[thread_id]
           );

        #pragma omp critical
        {
          if(!increased_tree_size) {
            tree_pool.eb(tour_current);
          }else{
            delete[] tour_current.data;
          }
        }
      }
#pragma omp critical
      { if(!L_tours_next.empty()) {
          L_tours_next.back().push(0);
        }
        while(!L_tours_next.empty()) {
          tours_next.eb(L_tours_next.back());
          L_tours_next.pop_back();
        }
        FORU(i,L_low,L_high) histogram[i] += L_histogram[i];
        FORU(i,L_low,L_high) L_histogram[i] = 0;
        low = min(low, L_low);
        high = max(high, L_high);
      }
    }

    f64 average_score = 0;
    { u64 total_count = 0;
      cutoff = max_score;
      cutoff_keep_probability = 1.0;
      FORU(i, low, high) {
        if(total_count+histogram[i] > config.width) {
          average_score += i * (config.width-total_count);
          cutoff = i;
          cutoff_keep_probability = (f32)(config.width-total_count) / (f32)(histogram[i]);
          total_count = config.width;
          break;
        }
        total_count += histogram[i];
        average_score += i * histogram[i];
      }
      FORU(i, low, high) histogram[i] = 0;
      average_score /= max<f64>(1, total_count);
       
    }

    i64 total_size = 0;
    for(auto const& t : tours_next) total_size += t.size;
    
    cerr << setw(6) << istep+1 <<
      ": scores = " << setw(3) << low << ".." << setw(3) << cutoff <<
      ": avg = " << fixed << setprecision(2) << average_score << 
      ", tree size = " << setw(11) << total_size <<
      ", num trees = " << setw(4) << tours_next.size() <<
      ", elapsed = " << setw(10) << fixed << setprecision(5) << timer_s.elapsed() << "s" <<
      endl;

    tours_current = tours_next;

    if(low == 0) {
      vector<u8> solution;
      
#pragma omp parallel
      {
        while(1) {
          euler_tour tour_current;
#pragma omp critical
          { if(!tours_current.empty()) {
              tour_current = tours_current.back();
              tours_current.pop_back();
            }else{
              tour_current.size = 0;
            }
          }
          if(tour_current.size == 0) break;
          
          auto lsolution = find_solution
            (low, P, initial_state, initial_direction,
             istep,
             tour_current);
          if(!lsolution.empty()) {
            #pragma omp critical
            {
              solution = lsolution;
            }
          }
        }
      }

      vector<beam_state> saved_states;
      FOR(i, num_threads) {
        saved_states.insert(end(saved_states), all(L_saved_states[i]));
      }
      
      beam_search_result result {
        .solution = solution,
        .saved_states = saved_states,
      };

      return result;
    }

  }
}
