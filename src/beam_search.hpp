#pragma once
#include "header.hpp"
#include "puzzle.hpp"

vector<u8> beam_search
(puzzle_data const& P,
 puzzle_state const& initial_state,
 u8 initial_direction,
 u64 width);

