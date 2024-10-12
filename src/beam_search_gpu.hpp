#pragma once
#include "header.hpp"
#include "base.hpp"

namespace beam_search_gpu {

  vector<u8> beam_search
  (puzzle_data const& P,
   puzzle_state const& initial_state,
   u8 initial_direction,
   i64 width);
}
