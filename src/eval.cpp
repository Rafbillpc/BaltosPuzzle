#include "eval.hpp"

weights_t weights;

void weights_t::init(){
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    dist_weight[u][v] = puzzle.dist_eval[u][v];
    // if(dist_weight[u][v] <= 23) dist_weight[u][v] = 0;
    // dist_weight[u][v] = puzzle.dist[u][v];
    // if(dist_weight[u][v] <= 1) dist_weight[u][v] = 0;
  }
}

void init_eval() {
  weights.init();
}
