#include "header.hpp"
#include <omp.h>
#include <argparse/argparse.hpp>

// void solve(puzzle_data const& puzzle,
//            puzzle_state initial_state,
//            u8 initial_directions,
//            i64 width) {
//   beam_search_config config {
//     .print = true,
//     .width = (u64)width,
//     .save_states = false,
//     .save_states_probability = 0.0,
//     .max_steps = 100'000'000,
//   };

//   weights_t weights;
//   weights.init();

//   beam_state state;
//   state.reset(puzzle, weights, initial_state, initial_directions);
  
//   auto result = beam_search
//     (puzzle, weights, state, config);
//   auto solution = result.solution;
  
//   vector<char> L, R;
//   u8 last_direction_src = (initial_directions >> 0 & 1);
//   u8 last_direction_tgt = (initial_directions >> 1 & 1); 
//   for(auto m : solution) {
//     if(m < 6) {
//       if(last_direction_src == 0) {
//         L.pb('1'+(m+1)%6);
//       }else{
//         L.pb('A'+(m+6)%6);
//       }
      
//       last_direction_src ^= 1;
//     }else{
//       m -= 6;

//       if(last_direction_tgt == 0) {
//         R.pb('A'+(m+3)%6);
//       }else{
//         R.pb('1'+(m+4)%6);
//       }
      
//       last_direction_tgt ^= 1;
//     }
//   }

//   reverse(all(R));
//   L.insert(end(L),all(R));

//   auto cost = L.size();
//   auto filename = "solutions/" + to_string(puzzle.n) + "/" + to_string(cost); 
  
//   ofstream out(filename);
//   out << puzzle.n << ":";
//   for(auto c : L) {
//     out << c;
//   }
//   out << endl;
//   out.close();
// }

int main(int argc, char** argv) {
  // runtime_assert(argc == 4);

  // i64 sz = atoi(argv[1]);
  // i64 width = atoll(argv[2]);
  // i32 initial_directions = atoi(argv[3]);
  // debug(sz, width);
  
  // automaton::init();
  // reachability.init();
  
  // auto C = load_configurations();

  // runtime_assert(C.count(sz));
  // unique_ptr<puzzle_data> P = make_unique<puzzle_data>();
  // P->make(sz);
  // solve(*P, C[sz], initial_directions, width);

  return 0;
}
