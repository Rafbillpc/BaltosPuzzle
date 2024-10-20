#pragma once
#include "header.hpp"

struct weights_t {
  f32 dist_weights[27*27];
  f32 nei_weights[7];

  void init() {
    FOR(i, 27) FOR(j, 27) {
      dist_weights[i*27+j] = 10*(i+j);
    }
    nei_weights[0] = 0;
    nei_weights[1] = 1;
    nei_weights[2] = 2;
    nei_weights[3] = 3;
    nei_weights[4] = 4;
    nei_weights[5] = 5;
    nei_weights[6] = 6;
  }
};
