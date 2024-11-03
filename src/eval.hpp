#pragma once
#include "puzzle.hpp"

const f64 EVAL_SCALE = 256;

const u32 NUM_FEATURES_DIST = 196;
const u32 NUM_FEATURES_NEI = 28;
const u32 NUM_FEATURES_SRC_EDGE = 1849;
const u32 NUM_FEATURES_TOK_PAIR = 1849;
const u32 NUM_FEATURES_TGT_EDGE = 1849;

const u32 NUM_FEATURES =
  NUM_FEATURES_DIST +
  NUM_FEATURES_NEI +
  NUM_FEATURES_SRC_EDGE +
  NUM_FEATURES_TGT_EDGE +
  NUM_FEATURES_TOK_PAIR;

const u32 MAX_REDUCTION = 3;
const u32 NUM_REDUCED = 43;

extern u32 dist_reduced_key[6][27][27];

using weights_vec = array<f64, NUM_FEATURES>;
using features_vec = array<i32, NUM_FEATURES>;

extern u32 dist_feature_key[27][27];
extern u32 nei_feature_key[1<<7];
extern u32 src_edge_feature_key[NUM_REDUCED][NUM_REDUCED];
extern u32 tgt_edge_feature_key[NUM_REDUCED][NUM_REDUCED];
extern u32 tok_pair_feature_key[NUM_REDUCED][NUM_REDUCED];

struct weights_t {
  u32 dist_weight[MAX_SIZE][MAX_SIZE];
  u32 nei_weight[1<<7];
  u32 src_edge_weight[1][1];
  u32 tgt_edge_weight[1][1];
  u32 tok_pair_weight[1][1];

  void init();
  void from_weights(weights_vec const& w);
};

extern weights_t weights;

void init_eval();
