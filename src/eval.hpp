#pragma once
#include "puzzle.hpp"

const f32 EVAL_SCALE = 64;

const u32 NUM_FEATURES_DIST = 196;
// const u32 NUM_FEATURES_NEI = 0;

const u32 NUM_FEATURES =
  NUM_FEATURES_DIST;

using weights_vec = array<f64, NUM_FEATURES>;
using features_vec = array<i64, NUM_FEATURES>;

inline u32 dist_feature_key[MAX_SIZE][MAX_SIZE];

struct weights_t {
  u32 dist_weight[MAX_SIZE][MAX_SIZE];

  void init();
  void from_weights(weights_vec const& t);
};

inline weights_t weights;

void init_eval();
