#include "header.hpp"
#include "puzzle.hpp"
#include "beam_search.hpp"
#include <omp.h>

void init_rng() {
  u64 seed = time(0);
  
#pragma omp parallel
  {
    u64 offset = omp_get_thread_num();
    rng.reset(seed + offset);
  }
}

void solve(puzzle_data const& P,
           puzzle_state initial_state,
           u8 initial_directions,
           i64 width) {
  auto solution = beam_search
    (P, initial_state, initial_directions, width);
  
  vector<char> L, R;
  u8 last_direction_src = (initial_directions >> 0 & 1);
  u8 last_direction_tgt = (initial_directions >> 1 & 1); 
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
  auto filename = "solutions/" + to_string(P.n) + "/" + to_string(cost); 
  
  ofstream out(filename);
  out << P.n << ":";
  for(auto c : L) {
    out << c;
  }
  out << endl;
  out.close();
}

int main(int argc, char** argv) {
  runtime_assert(argc == 4);

  i64 sz = atoi(argv[1]);
  i64 width = atoll(argv[2]);
  i32 initial_directions = atoi(argv[3]);
  debug(sz, width);
  
  init_rng();
  automaton::init();
  
  auto C = load_configurations();

  runtime_assert(C.count(sz));
  unique_ptr<puzzle_data> P = make_unique<puzzle_data>();
  P->make(sz);
  solve(*P, C[sz], initial_directions, width);

  return 0;
}
