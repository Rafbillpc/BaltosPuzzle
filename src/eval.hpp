#pragma once
#include "puzzle.hpp"

const u32 NUM_FEATURES_DIST = 196;

struct weights_t {
  u32 dist_weight[MAX_SIZE][MAX_SIZE];

  void init();
};

extern weights_t weights;

void init_eval();
