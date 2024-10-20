#include "beam_state.hpp"
#include "header.hpp"
#include "puzzle.hpp"
#include "beam_state.hpp"
#include "beam_search.hpp"

void training_loop
(puzzle_data const& puzzle,
 puzzle_state const& initial_state,
 i64 width,
 i64 training_width)
{
  beam_search_config config {
    .width = (u64)width,
    .save_states = true,
    .save_states_probability = 0.001,
  };

  weights_t weights;
  weights.init();
  
  beam_state state;
  state.reset(puzzle, weights, initial_state, 0);
  
  auto result = beam_search
    (puzzle,
     weights,
     state,
     config);

  debug(result.saved_states.size());
}

int main(int argc, char** argv) {
  runtime_assert(argc == 4);

  i64 sz = atoi(argv[1]);
  i64 width = atoll(argv[2]);
  i64 training_width = atoll(argv[2]);
  
  automaton::init();
  reachability.init();
  
  auto C = load_configurations();

  runtime_assert(C.count(sz));
  unique_ptr<puzzle_data> P = make_unique<puzzle_data>();
  P->make(sz);
  // solve(*P, C[sz], initial_directions, width);

  training_loop(*P, C[sz], width, training_width);
  
  return 0;
}
