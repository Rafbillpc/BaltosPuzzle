#pragma once
#include "header.hpp"
#include "eval.hpp"
#include <omp.h>

struct training_config {
  u32   steps;
  bool   print;
  u32    gather_width;
  u32    gather_count;
  f32    features_save_probability;
  u32   training_iters;
  string output;
};

struct training_sample {
  features_vec features;

  training_sample(){}
  training_sample(features_vec const& v1, features_vec const& v2) {
    FOR(i, NUM_FEATURES) features[i] = v1[i] - v2[i];
  }
};

vector<training_sample> gather_samples
(training_config const& config);

void update_weights
(training_config const& config,
 vector<training_sample> const& samples);

void training_loop
(training_config const& config);
