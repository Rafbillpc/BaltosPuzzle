#include "eval.hpp"

void weights_t::init(){
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    dist_weight[u][v] = puzzle.dist_eval[u][v];
  }
}

void weights_t::from_weights(weights_vec const& w) {
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    auto p = puzzle.dist_pair[u][v];
    dist_weight[u][v] = EVAL_SCALE * w[dist_feature_key[p[0]][p[1]]];
  }
}

void init_features() {
  i32 next_feature = 0;
  FOR(x, 27) FOR(y, x+1) if(x+y < 27) {
    dist_feature_key[x][y] = next_feature++;
  }
  runtime_assert(next_feature == NUM_FEATURES_DIST);

}

void init_eval() {
  init_features();
  weights.init();
}
