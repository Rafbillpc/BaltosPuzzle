#include "solver.hpp"
#include "beam_search.hpp"
#include <omp.h>

void save_solution
(u32 initial_directions,
 vector<u8> const& solution)
{
  vector<char> L, R;
  u8 last_direction_src = (initial_directions >> 0) & 1;
  u8 last_direction_tgt = (initial_directions >> 1) & 1; 
  for(auto m : solution) {
    if(m < 6) {
      if(last_direction_src == 0) {
        L.pb('1'+(m+1)%6);
      }else{
        L.pb('A'+(m+6)%6);
      }
      
      last_direction_src ^= 1;
    }else{
      m -= 6;

      if(last_direction_tgt == 0) {
        R.pb('A'+(m+3)%6);
      }else{
        R.pb('1'+(m+4)%6);
      }
      
      last_direction_tgt ^= 1;
    }
  }

  reverse(all(R));
  L.insert(end(L),all(R));

  auto cost = L.size();
  auto filename = "solutions/" + to_string(puzzle.n) + "/" + to_string(cost); 
  
  ofstream out(filename);
  out << puzzle.n << ":";
  for(auto c : L) {
    out << c;
  }
  out << endl;
  out.close();
}

void solve
(puzzle_state const& initial_state,
 u32 width,
 u32 dirs) {

  auto search = make_unique<beam_search>(beam_search_config {
      .print = true,
      .print_interval = 1,
      .width = width,
      .features_save_probability = 0.0,
      .num_threads = (u32)omp_get_max_threads()
    });

  beam_state state;
  state.src = initial_state;
  state.src.direction = (dirs >> 0) & 1;
  state.tgt.set_tgt();
  state.tgt.direction = (dirs >> 1) & 1;
  state.init();
  
  auto result = search->search(state);
  save_solution(dirs, result.solution);
}

