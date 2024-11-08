#include "beam_search.hpp"
#include "puzzle.hpp"
#include <mutex>
#include <omp.h>

const u64 HASH_SIZE = 1ull<<28;
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
        if(S.is_solved()) {
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

  f32 cutoff_heur_running = 1.0 + rng.randomDouble();

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
        bool keep = v < cutoff_heur;
        if(v == cutoff_heur) {
          cutoff_heur_running += cutoff_heur_keep_probability;
          if(cutoff_heur_running >= 1.0) {
            keep = 1;
            cutoff_heur_running -= 1.0;
          }
        }
        if(keep) {
          while(ncommit < nstack_moves) {
            tour_next->push(1+stack_moves[ncommit]);
            ncommit += 1;
          }

          FOR(m, 12) if(m != stack_last_move_src[nstack_moves] &&
                        m != stack_last_move_tgt[nstack_moves]) {
            auto [v,h,solved] = S.plan_move(m);
            auto prev = hash_table[h&HASH_MASK];
            if(prev != h) {
              hash_table[h&HASH_MASK] = h;
              if(solved) found_solution = true;
              low_heur = min(low_heur, v);
              high_heur = max(high_heur, v);
              histogram_heur[v] += 1;
              tour_next->push(1+m);
              tour_next->push(0);
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
  should_stop = false;

  L_histograms_heur.resize(config.num_threads);
  L_instances.resize(config.num_threads);
}

beam_search::~beam_search() {
}

beam_search_result
beam_search::search(beam_state const& initial_state) {
  u32 max_heur = initial_state.value() * 1.2 + 1024;
  if(histogram_heur.size() < max_heur) histogram_heur.resize(max_heur);

  vector<euler_tour> tours_current;
  tours_current.eb(get_new_tree());
  tours_current.back().push(0);

  u32 cutoff_heur = max_heur;
  f32 cutoff_heur_keep_probability = 1.0;

  for(u32 istep = 0;; ++istep) {
    if(should_stop || istep > MAX_SOLUTION_SIZE - 10) {
      debug("FAIL");
      return beam_search_result{};
    }
    
    timer timer_s;

    u32 low_heur = max_heur, high_heur = 0;
    bool found_solution = false;

    {
      sort(all(tours_current), [&](auto const& t1, auto const& t2) {
        return t1.size < t2.size;
      });
      
      vector<euler_tour> tours_next;

#pragma omp parallel num_threads(config.num_threads)
      {
        u32 thread_id = omp_get_thread_num();

        auto &L_histogram_heur = L_histograms_heur[thread_id];
        if(L_histogram_heur.size() < max_heur) L_histogram_heur.resize(max_heur, 0);
        auto &L_instance = L_instances[thread_id];

        vector<euler_tour> L_tours_next;

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
          
          L_instance.hash_table = hash_table.data();
          L_instance.histogram_heur = L_histogram_heur.data();
          L_instance.istep = istep;
          L_instance.cutoff_heur = cutoff_heur;
          L_instance.cutoff_heur_keep_probability = cutoff_heur_keep_probability;
          L_instance.low_heur = max_heur;
          L_instance.high_heur = 0;
          L_instance.found_solution = false;
        
          L_instance.traverse_tour
            (config, initial_state, tour_current, L_tours_next);

#pragma omp critical
          {
            free_tree(tour_current);
            low_heur = min(low_heur, L_instance.low_heur);
            high_heur = max(high_heur, L_instance.high_heur);
            found_solution = found_solution || L_instance.found_solution;
          }
        }

        #pragma omp critical
        {
          FORU(i, low_heur, high_heur) {
            histogram_heur[i] += L_histogram_heur[i];
            L_histogram_heur[i] = 0;
          }
          tours_next.insert(end(tours_next), all(L_tours_next));
        }
      }
      
      tours_current = tours_next;
    }
    
    f64 average_heur = 0;
    { u64 total_count = 0;
      cutoff_heur = max_heur;
      cutoff_heur_keep_probability = 1.0;
      FORU(i, low_heur, high_heur) {
        if(total_count + histogram_heur[i] > config.width) {
          average_heur += (f64) i * (config.width-total_count);
          cutoff_heur = i;
          cutoff_heur_keep_probability = (f32)(config.width-total_count) / (f32)(histogram_heur[i]);
          total_count = config.width;
          break;
        }
        total_count += histogram_heur[i];
        average_heur += (f64) i * histogram_heur[i];
      }
      FORU(i, low_heur, high_heur) histogram_heur[i] = 0;
      average_heur /= max<f64>(1, total_count);
    }
   
    i64 total_size = 0;
    for(auto tour : tours_current) {
      total_size += tour.size;
    }

    if(config.print && (istep % config.print_interval == 0)) {
#pragma omp critical
      {
        cerr << setw(6) << istep+1 <<
          ": heur = " << setw(6) << low_heur << ".." << setw(6) << cutoff_heur <<
          ", tree size = " << setw(11) << total_size <<
          ", tree count = " << setw(4) << tours_current.size() <<
          ", elapsed = " << setw(10) << fixed << setprecision(5) << timer_s.elapsed() << "s" <<
          endl;
      }
    }

    if(found_solution) {
      vector<u8> solution;
      
      for(auto tour_current : tours_current) {
        find_solution(solution, istep+1, initial_state, tour_current);
        if(!solution.empty()) break;
      }

      runtime_assert(!solution.empty());

      beam_state T = initial_state;
      for(auto move : solution) T.do_move(move);

      T.print();
      
      return beam_search_result {
        .solution = solution
      };
    }
  }
}
