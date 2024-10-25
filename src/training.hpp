#pragma once
#include "header.hpp"
#include "beam_search.hpp"
#include "puzzle.hpp"
#include <omp.h>

struct training_sample {
  features_vec features1;
  features_vec features2;
};

vector<training_sample> gather_samples();
void update_weights(vector<training_sample> const& samples);
