#pragma once
#include "header.hpp"
#include "puzzle.hpp"
#include "beam_state.hpp"

struct beam_search_config {
  u64 width;
  bool save_states;
  f32 save_states_probability;
};

struct beam_search_result {
  vector<u8> solution;
  vector<beam_state> saved_states;
};

beam_search_result beam_search
(puzzle_data const& P,
 puzzle_state const& initial_state,
 u8 initial_direction,
 beam_search_config config);

