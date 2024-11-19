#include "eval.hpp"

void weights_t::init(){
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    dist_weight[u][v] = puzzle.dist_eval[u][v];
  }
  FOR(m, 1<<6) {
    nei_weight[m] = 0;
  }
  cycle2_weight = 0;
  cycle3_weight = 0;
}

void weights_t::from_weights(weights_vec const& w) {
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    auto p = puzzle.dist_pair[u][v];
    dist_weight[u][v] = EVAL_SCALE * w[dist_feature_key[p[0]][p[1]]];
  }
  FOR(m, bit(6)) {
    nei_weight[m] = EVAL_SCALE * w[nei_feature_key[m]];
  }
  cycle2_weight = EVAL_SCALE * w[cycle2_feature_key];
  cycle3_weight = EVAL_SCALE * w[cycle3_feature_key];
}

void init_features() {
  i32 next_feature = 0;
  FOR(x, 27) FOR(y, x+1) if(x+y < 27) {
    dist_feature_key[x][y] = next_feature++;
  }
  runtime_assert(next_feature == NUM_FEATURES_DIST);
  
  FOR(mask, bit(6)) {
    bool bits[6];
    FOR(i, 6) bits[i] = (mask&bit(i)?1:0);
    u32 min_mask = mask;
    FOR(r, 6) {
      u32 mask2 = 0;
      FOR(i, 6) if(bits[(i+r)%6]) mask2 |= bit(i);
      min_mask = min(min_mask, mask2);
    }
    FOR(r, 6) {
      u32 mask2 = 0;
      FOR(i, 6) if(bits[(6-i + r)%6]) mask2 |= bit(i);
      min_mask = min(min_mask, mask2);
    }
    if(min_mask == (u32)mask) {
      nei_feature_key[mask] = next_feature++;
    }else{
      nei_feature_key[mask] = nei_feature_key[min_mask];
    }
  }
  runtime_assert(next_feature ==
                 NUM_FEATURES_DIST +
                 NUM_FEATURES_NEI);

  runtime_assert(next_feature == cycle2_feature_key);
  next_feature++;
  runtime_assert(next_feature == cycle3_feature_key);
  next_feature++;
  
  runtime_assert(next_feature ==
                 NUM_FEATURES);
}

void init_eval() {
  init_features();
  weights.init();
}
