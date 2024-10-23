#include "header.hpp"
#include "beam_search.hpp"
#include "puzzle.hpp"
#include <omp.h>

struct training_sample {
  vector<i32> features;
  i32         goal;
};

int main(int argc, char** argv) {
  puzzle.make(4);

  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    weights.dist_weight[u][v] = 10 * puzzle.dist[u][v];
  }
  
  unique_ptr<beam_search> search = make_unique<beam_search>(beam_search_config {
      .print = false,
      .width = 1<<10,
    });

  vector<training_sample> samples;
  
  while(1) {
    auto seed = rng.randomInt32();
    debug(seed);
    puzzle_state src;
    src.generate(seed);

    beam_state state;
    state.src = src;
    state.src.direction = rng.random32(2);
    state.tgt.set_tgt();
    state.tgt.direction = rng.random32(2);
    state.init();
    
    auto result = search->search(state);

    auto size = result.solution.size();
    FOR(i, size) {
      // sample.pb(training_sample {
      //   .features = state.features(),
      //   .goal = size - i,
      // });
      state.do_move(result.solution[i]);
    }
  }

  return 0;
}
