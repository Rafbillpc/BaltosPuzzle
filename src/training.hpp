#pragma once
#include "header.hpp"
#include "eval.hpp"
#include <omp.h>

struct training_config {
  bool   print;
  u32    gather_width;
  u32    gather_count;
  f32    features_save_probability;
  string output;
};

struct training_sample {
  features_vec features1;
  features_vec features2;
};

vector<training_sample> gather_samples
(training_config const& config);

void update_weights
(training_config const& config,
 vector<training_sample> const& samples);

void training_loop
(training_config const& config);
