#pragma once
#include "puzzle.hpp"

const u32 NUM_FEATURES_DIST = 196;

const u32 NUM_FEATURES =
  NUM_FEATURES_DIST;

using features_vec = array<i32, NUM_FEATURES>;

extern u32 dist_feature_key[27][27];

struct weights_t {
  u32 dist_weight[MAX_SIZE][MAX_SIZE];

  void init();
};

extern weights_t weights;

void init_eval();
