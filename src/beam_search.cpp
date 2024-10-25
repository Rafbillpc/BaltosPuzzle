#include "beam_search.hpp"
#include "puzzle.hpp"
#include <mutex>

const u64 HASH_SIZE = 1ull<<25;
const u64 HASH_MASK = HASH_SIZE-1;

const i64 MIN_TREE_SIZE = 1<<17;
i64 tree_size = MIN_TREE_SIZE;

mutex bs_mutex;

vector<euler_tour> tree_pool;

euler_tour get_new_tree(){
  euler_tour tour;

  { lock_guard<mutex> lock(bs_mutex);
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

void free_tree(euler_tour tree) {
  lock_guard<mutex> lock(bs_mutex);
  tree_pool.pb(tree);
}


void find_solution
(vector<u8> &solution,
 u32 istep,
 beam_state S,
 euler_tour const& tour_current
 )
{
  vector<u8> stack_moves(istep);
  u32 nstack_moves = 0;

  FOR(iedge, tour_current.size) {
    u8 edge = tour_current[iedge];
    if(edge > 0) {
      stack_moves[nstack_moves] = edge-1;
      S.do_move(edge-1);
      nstack_moves += 1;
    }else{
      if(nstack_moves == istep) {
        auto v = S.value();
        if(v == 0) {
          solution = stack_moves;
          return;
        }
      }
        
      if(nstack_moves == 0) {
        break;
      }
				
      nstack_moves -= 1;
      S.undo_move(stack_moves[nstack_moves]);
    }
  }
}



void beam_search_instance::traverse_tour
(beam_search_config const& config,
 beam_state S,
 euler_tour const& tour_current,
 vector<euler_tour> &tours_next)
{
  u32 nstack_moves = 0;
  stack_last_move_src[0] = 12;
  stack_last_move_tgt[0] = 12;

  u32 ncommit = 0;
  if(tours_next.empty()) tours_next.eb(get_new_tree());
  auto *tour_next = &tours_next.back(); 
  runtime_assert(tour_next->max_size == tree_size);

  f32 cutoff_running = 1.0 + rng.randomDouble();

  FOR(iedge, tour_current.size) {
    u8 edge = tour_current[iedge];
    if(edge > 0) {
      stack_moves[nstack_moves] = edge-1;
      S.do_move(edge-1);
      if(edge-1 < 6) {
        stack_last_move_src[nstack_moves+1] = edge-1;
        stack_last_move_tgt[nstack_moves+1] = stack_last_move_tgt[nstack_moves];
      }else{
        stack_last_move_src[nstack_moves+1] = stack_last_move_src[nstack_moves];
        stack_last_move_tgt[nstack_moves+1] = edge-1;
      }
      nstack_moves += 1;
    }else{
      if(nstack_moves == istep) {
        auto v = S.value();
        bool keep = v < cutoff;
        if(v == cutoff) {
          cutoff_running += cutoff_keep_probability;
          if(cutoff_running >= 1.0) {
            keep = 1;
            cutoff_running -= 1.0;
          }
        }
        if(keep) {

          if(rng.randomFloat() < config.features_save_probability){
            saved_features->eb();
            get<0>(saved_features->back()) = istep;
            S.features(get<1>(saved_features->back()));
          }
          
          while(ncommit < nstack_moves) {
            tour_next->push(1+stack_moves[ncommit]);
            ncommit += 1;
          }

          FOR(m, 12) if(m != stack_last_move_src[nstack_moves] &&
                        m != stack_last_move_tgt[nstack_moves]) {
            auto [v,h] = S.plan_move(m);
            if(1 || v <= cutoff) {
              auto prev = hash_table[h&HASH_MASK];
              if(prev != h) {
                hash_table[h&HASH_MASK] = h;
                low = min(low, v);
                high = max(high, v);
                histogram[v] += 1;
                tour_next->push(1+m);
                tour_next->push(0);
              }
            }
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
      S.undo_move(stack_moves[nstack_moves]);
    }
    
    if(__builtin_expect(tour_next->size + 2 * istep + 128 > tree_size, false)) {
      FORD(i,ncommit-1,0) tour_next->push(0);
      tour_next->push(0);
      tours_next.eb(get_new_tree());
      tour_next = &tours_next.back();
      ncommit = 0;
    }
  }
}

beam_search::beam_search(beam_search_config config_) {
  config = config_;
  hash_table.assign(HASH_SIZE, rng.randomInt64());
}

beam_search::~beam_search() {
}

beam_search_result
beam_search::search(beam_state const& initial_state) {
  u32 max_score = initial_state.value() * 1.2 + 1024;
  if(histogram.size() < max_score) histogram.resize(max_score);

  vector<euler_tour> tours_current;
  tours_current.eb(get_new_tree());
  tours_current.back().push(0);

  u32 cutoff = max_score;
  f32 cutoff_keep_probability = 1.0;

  u32 low = max_score, high = 0;

  vector<tuple<i32, features_vec > > saved_features;
  
  for(u32 istep = 0;; ++istep) {
    if(istep > MAX_SOLUTION_SIZE - 10) {
      debug("FAIL");
      return beam_search_result{};
    }
    
    timer timer_s;

    saved_features.eb();
    
    {
      vector<euler_tour> tours_next;
      
      for(auto tour_current : tours_current) {
        instance.hash_table = hash_table.data();
        instance.histogram = histogram.data();
        instance.istep = istep;
        instance.cutoff = cutoff;
        instance.cutoff_keep_probability = cutoff_keep_probability;
        instance.low = max_score;
        instance.high = 0;
        instance.saved_features = &saved_features;
        
        instance.traverse_tour(config, initial_state, tour_current, tours_next);

        free_tree(tour_current);
        low = min(low, instance.low);
        high = max(high, instance.high);
      }
      tours_current = tours_next;
    }
    
    f64 average_score = 0;
    { u64 total_count = 0;
      cutoff = max_score;
      cutoff_keep_probability = 1.0;
      FORU(i, low, high) {
        if(total_count + histogram[i] > config.width) {
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
    for(auto tour : tours_current) {
      total_size += tour.size;
    }

    if(config.print && (istep % config.print_interval == 0)) {
#pragma omp critical
      {
        cerr << setw(6) << istep+1 <<
          ": low..cut = " << setw(3) << low << ".." << setw(3) << cutoff <<
          ": avg = " << fixed << setprecision(2) << average_score << 
          ", tree size = " << setw(11) << total_size <<
          ", elapsed = " << setw(10) << fixed << setprecision(5) << timer_s.elapsed() << "s" <<
          endl;
      }
    }

    if(low == 0) {
      vector<u8> solution;
      
      for(auto tour_current : tours_current) {
        find_solution(solution, istep+1, initial_state, tour_current);
        if(!solution.empty()) break;
      }

      return beam_search_result {
        .solution = solution,
        .saved_features = saved_features,
      };
    }
  }
}
