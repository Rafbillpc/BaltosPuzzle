#pragma once
#include "header.hpp"

const i32 du[6] = {0,1,1,0,-1,-1};
const i32 dv[6] = {1,1,0,-1,-1,0};

const u32 MAX_SIZE = 2107;
const u32 MAX_SOLUTION_SIZE = 50'000;

static inline
f32 dist_heuristic(f32 x, f32 y) {
  const f32 a = 0.72813342;
  const f32 b = 1.08982143;
  const f32 c = 0.36606469;
  const f32 d = 0.15821429;
  const f32 e = 0.30125;

  return a*x+b*y+c*x*x+d*x*y+e*y*y;
}

struct puzzle_data {
  i32 n;
  u32 size;
  u32 center;

  u32 rot[MAX_SIZE][6];
  u32 dist[MAX_SIZE][MAX_SIZE];
  array<i32, 2> dist_delta[MAX_SIZE][MAX_SIZE];
  u32 dist_key[MAX_SIZE][MAX_SIZE];
  f32 dist_heuristic_initial[MAX_SIZE][MAX_SIZE];
  
  array<i32, 2> to_coord[MAX_SIZE];
  map<array<i32, 2>, u32> from_coord;

  i32 tgt_tok_to_pos[MAX_SIZE];
  i32 tgt_pos_to_tok[MAX_SIZE];

  u32 num_features;
  
  void make(i32 n);
};

extern puzzle_data puzzle;

struct puzzle_state {
  u32 pos_to_tok[MAX_SIZE];
  u32 tok_to_pos[MAX_SIZE];
  u8  direction = 0;

  bool get_parity() const;
  
  void set_tgt();
  void print() const;

  void do_move(u8 move) {
    u32 a = tok_to_pos[0], b, c;
    
    if(direction == 0) {
      b = puzzle.rot[a][move];
      c = puzzle.rot[a][(move+1)%6];
    }else{
      b = puzzle.rot[a][move];
      c = puzzle.rot[a][(move+5)%6];
    }

    u32 xb = pos_to_tok[b];
    u32 xc = pos_to_tok[c];

    pos_to_tok[a]  = xc;
    pos_to_tok[b]  = 0;
    pos_to_tok[c]  = xb;
    tok_to_pos[0]  = b;
    tok_to_pos[xb] = c;
    tok_to_pos[xc] = a;
  }

  void generate(u64 seed);
};
   
map<i32, puzzle_state> load_configurations();



