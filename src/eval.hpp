#pragma once
#include "puzzle.hpp"

const f64 EVAL_SCALE = 256;
const u32 NUM_FEATURES_DIST = 196;
const u32 NUM_FEATURES_NEI = 28;
const u32 NUM_FEATURES_POUET = 10;

const u32 NUM_FEATURES =
  NUM_FEATURES_DIST +
  NUM_FEATURES_NEI;

using weights_vec = array<f64, NUM_FEATURES>;
using features_vec = array<i32, NUM_FEATURES>;

extern u32 dist_feature_key[27][27];
extern u32 nei_feature_key[1<<7];

struct weights_t {
  u32 dist_weight[MAX_SIZE][MAX_SIZE];
  u32 nei_weight[1<<7];

  void init();
  void from_weights(weights_vec const& w);
};

extern weights_t weights;

void init_eval();
