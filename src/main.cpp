#include "header.hpp"
#include "puzzle.hpp"
#include "training.hpp"
#include <omp.h>

int main(int argc, char** argv) {
  puzzle.make(12);
  weights.init();

  while(1) {
    auto samples = gather_samples();
    update_weights(samples);
  }
  
  return 0;
}
