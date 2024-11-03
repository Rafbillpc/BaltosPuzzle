#include "eval.hpp"

const f32 initial_dist_heuristic[NUM_FEATURES_DIST] =
  {
    0.00, 
    1.00, 2.18, 
    2.65, 4.05, 6.09, 
    4.87, 6.39, 8.50, 11.17, 
    7.48, 9.28, 11.52, 14.27, 17.45, 
    10.52, 12.48, 14.85, 17.72, 20.98, 24.55, 
    14.02, 16.07, 18.61, 21.68, 24.99, 28.61, 32.74, 
    17.76, 20.27, 22.93, 26.03, 29.48, 33.29, 37.54, 42.27, 
    22.39, 24.82, 27.81, 31.03, 34.44, 38.61, 42.79, 47.74, 53.49, 
    27.20, 29.94, 33.08, 36.44, 40.08, 44.23, 48.91, 53.98, 59.54, 66.17, 
    32.94, 35.99, 39.35, 42.66, 46.48, 50.93, 55.68, 60.75, 66.45, 73.27, 80.91, 
    39.65, 42.27, 45.80, 49.51, 53.54, 58.25, 63.36, 68.77, 74.93, 81.14, 89.50, 97.08, 
    46.25, 49.72, 53.26, 57.66, 61.53, 66.57, 71.63, 77.47, 84.05, 90.85, 98.57, 107.92, 117.56, 
    53.36, 57.69, 61.52, 66.02, 70.38, 75.97, 80.89, 87.70, 93.83, 101.62, 109.54, 120.40, 132.74, 144.91, 
    62.44, 66.22, 71.10, 75.60, 81.04, 86.16, 91.94, 98.43, 105.33, 114.45, 123.35, 134.14, 145.19, 
    71.91, 75.93, 81.82, 86.54, 91.42, 96.86, 103.37, 110.57, 119.48, 128.03, 138.22, 151.43, 
    83.09, 87.77, 93.67, 98.54, 103.80, 109.97, 116.33, 125.40, 133.35, 145.35, 156.73, 
    95.10, 100.82, 106.48, 112.61, 118.13, 124.30, 132.38, 141.99, 151.61, 163.18, 
    109.35, 115.91, 121.68, 128.02, 134.47, 141.63, 150.47, 160.76, 171.70, 
    125.79, 132.44, 137.87, 145.23, 152.06, 160.57, 170.30, 180.28, 
    143.01, 150.88, 157.98, 164.91, 172.91, 181.56, 192.77, 
    164.29, 171.72, 179.05, 186.21, 195.92, 206.26, 
    187.83, 194.11, 202.80, 211.08, 220.31, 
    211.52, 218.80, 226.57, 236.28, 
    238.50, 244.82, 253.19, 
    265.16, 272.01, 
    293.62
  };

u32 dist_feature_key[27][27];
u32 dist_reduced_key[6][27][27];
u32 nei_feature_key[1<<7];
weights_t weights;

void weights_t::init(){
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    dist_weight[u][v] =
      EVAL_SCALE * initial_dist_heuristic[puzzle.dist_feature[u][v]];
  }

  FOR(i, bit(7)) {
    nei_weight[i] = 0;
  }
}

void weights_t::from_weights(weights_vec const& w) {
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    dist_weight[u][v] =
      EVAL_SCALE * w[puzzle.dist_feature[u][v]];
  }

  FOR(i, bit(7)) {
    nei_weight[i] = EVAL_SCALE * w[nei_feature_key[i]];
  }
}

void init_eval() {
  i32 next_feature = 0;
  FOR(x, 27) FOR(y, x+1) if(x+y < 27) {
    dist_feature_key[x][y] = next_feature++;
  }
  runtime_assert(next_feature == NUM_FEATURES_DIST);
   
  i32 next_reduced = 0;
  FOR(d, 6) FOR(a, 27) FOR(b, 27) dist_reduced_key[d][a][b] = (u32)-1;
  {
    i32 x = next_reduced++;
    FOR(d, 6) dist_reduced_key[d][0][0] = x;
  }
  FORU(a, 1, MAX_REDUCTION) {
    i32 x = next_reduced++;
    FOR(d, 6) {
      dist_reduced_key[d][a][a] = x;
    }
  }
  FORU(a, MAX_REDUCTION+1, 26) {
    FOR(d, 6) dist_reduced_key[d][a][a] = dist_reduced_key[d][MAX_REDUCTION][MAX_REDUCTION];
  }
  FORU(a, 1, MAX_REDUCTION) {
    i32 x = next_reduced++;
    FOR(d, 6) {
      dist_reduced_key[d][0][a] = x;
      dist_reduced_key[(d+1)%6][a][0] = x;
    }
  }
  FORU(a, MAX_REDUCTION+1, 26) {
    FOR(d, 6) {
      dist_reduced_key[d][0][a] = dist_reduced_key[d][0][MAX_REDUCTION];
      dist_reduced_key[(d+1)%6][a][0] = dist_reduced_key[d][0][MAX_REDUCTION];
    }
  }
  FORU(a, 1, MAX_REDUCTION) FORU(b, 1, MAX_REDUCTION) if(a != b) {
    FOR(d, 6) {
      i32 x = next_reduced++;
      dist_reduced_key[d][a][b] = x;
    }
  }
  FOR(d, 6) FORU(a, 1, 26) FORU(b, 1, 26) if(dist_reduced_key[d][a][b] == (u32)-1) {
    dist_reduced_key[d][a][b]
      = dist_reduced_key[d][min<u32>(a, MAX_REDUCTION)][min<u32>(b, MAX_REDUCTION)];
  }
  FOR(d, 6) FOR(a, 27) FOR(b, 27) runtime_assert(dist_reduced_key[d][a][b] != (u32)-1);
  
  FOR(mask, bit(6)) {
    i32 min_mask = mask;
    FOR(s, 6) {
      i32 rotated_mask = ((mask<<s)|(mask>>(6-s))) & (bit(6)-1);
      if(rotated_mask < min_mask) min_mask = rotated_mask;
    }
    if(mask == min_mask) {
      nei_feature_key[mask] = next_feature++;
      nei_feature_key[mask|bit(6)] = next_feature++;
    }else{
      nei_feature_key[mask] = nei_feature_key[min_mask];
      nei_feature_key[mask|bit(6)] = nei_feature_key[min_mask|bit(6)];
    }
    auto f0 = nei_feature_key[mask] - NUM_FEATURES_DIST;
    auto f1 = nei_feature_key[mask|bit(6)] - NUM_FEATURES_DIST;
    debug(bitset<6>(mask), bitset<6>(min_mask), f0, f1);
  }
  debug(next_feature);
  runtime_assert(next_feature == NUM_FEATURES_DIST + NUM_FEATURES_NEI);
}
