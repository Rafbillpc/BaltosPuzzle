#include "eval.hpp"

weights_t weights;

void weights_t::init() {
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    weights.dist_weight[u][v] =
      64 * initial_dist_heuristic[puzzle.dist_feature[u][v]];
  }
}
