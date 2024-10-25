#pragma once
#include "header.hpp"
#include "puzzle.hpp"
#include "eval.hpp"

FORCE_INLINE
u64 hash_pos(u32 x, u32 y) {
  return uint64_hash::hash_int(x * 4096 + y);
}

struct beam_state {
  puzzle_state src, tgt;

  u32 cost;
  u64 hash;

  void init() {
    cost = 0;
    hash = rng.randomInt64();

    FORU(u, 1, puzzle.size-1) {
      add_dist(u);
    }

    // add cells
  }
  
  FORCE_INLINE
  void rem_dist(u32 u) {
    u32 x = src.tok_to_pos[u];
    u32 y = tgt.tok_to_pos[u];
    cost -= weights.dist_weight[x][y];
  }

  FORCE_INLINE
  void add_dist(u32 u) {
    u32 x = src.tok_to_pos[u];
    u32 y = tgt.tok_to_pos[u];
    cost += weights.dist_weight[x][y];
  }

  FORCE_INLINE
  void rem_cell(u32 u) {
  }
  
  FORCE_INLINE
  void add_cell(u32 u) {
  }

  FORCE_INLINE
  tuple<u32, u64> plan_move(u8 move) {
    do_move(move);
    u32 v = value();
    u64 h = hash;
    undo_move(move);
    return {v,h};
  }
  
  void do_move_src(u8 move) {
    u32 a = src.tok_to_pos[0], b, c;
    
    if(src.direction == 0) {
      b = puzzle.rot[a][move];
      c = puzzle.rot[a][(move+1)%6];
    }else{
      b = puzzle.rot[a][move];
      c = puzzle.rot[a][(move+5)%6];
    }

    u32 xb = src.pos_to_tok[b];
    u32 xc = src.pos_to_tok[c];

    rem_dist(xb);
    rem_dist(xc);
    hash ^= hash_pos(b, tgt.tok_to_pos[xb]);
    hash ^= hash_pos(c, tgt.tok_to_pos[xc]);
    rem_cell(b);
    rem_cell(c);

    src.pos_to_tok[a]  = xc;
    src.pos_to_tok[b]  = 0;
    src.pos_to_tok[c]  = xb;
    src.tok_to_pos[0]  = b;
    src.tok_to_pos[xb] = c;
    src.tok_to_pos[xc] = a;

    add_dist(xc);
    add_dist(xb);
    hash ^= hash_pos(c, tgt.tok_to_pos[xb]);
    hash ^= hash_pos(a, tgt.tok_to_pos[xc]);
    add_cell(c);
    add_cell(a);

    src.direction ^= 1;
  }

  void do_move_tgt(u8 move){
    u32 a = tgt.tok_to_pos[0], b, c;
      
    if(tgt.direction == 0) {
      b = puzzle.rot[a][move];
      c = puzzle.rot[a][(move+1)%6];
    }else{
      b = puzzle.rot[a][move];
      c = puzzle.rot[a][(move+5)%6];
    }

    u32 xb = tgt.pos_to_tok[b];
    u32 xc = tgt.pos_to_tok[c];
    
    rem_dist(xb);
    rem_dist(xc);    
    hash ^= hash_pos(src.tok_to_pos[xb], b);
    hash ^= hash_pos(src.tok_to_pos[xc], c);
    rem_cell(b);
    rem_cell(c);
    
    tgt.pos_to_tok[a]  = xc;
    tgt.pos_to_tok[b]  = 0;
    tgt.pos_to_tok[c]  = xb;
    tgt.tok_to_pos[0]  = b;
    tgt.tok_to_pos[xb] = c;
    tgt.tok_to_pos[xc] = a;

    add_dist(xc);
    add_dist(xb);
    hash ^= hash_pos(src.tok_to_pos[xb], c);
    hash ^= hash_pos(src.tok_to_pos[xc], a);
    add_cell(c);
    add_cell(a);

    tgt.direction ^= 1;
  }
  
  void do_move(u8 move) {
    if(move < 6) do_move_src(move);
    else do_move_tgt(move - 6);
  }

  void undo_move(u8 move) {
    do_move(6*(move/6) + (move+3)%6);
  }

  u32 value() const {
    return cost;
  }

  void features(features_vec& V) const {
    FOR(i, NUM_FEATURES) V[i] = 0;
    FORU(u, 1, puzzle.size-1) {
      u32 x = src.tok_to_pos[u];
      u32 y = tgt.tok_to_pos[u];
      V[puzzle.dist_feature[x][y]] += 1;
    }
  }
};

using euler_tour_edge = u8;
struct euler_tour {
  i64 max_size;
  i64 size;
  euler_tour_edge* data;
  
  FORCE_INLINE void reset() { size = 0; }
  FORCE_INLINE void push(i32 x) {
    data[size++] = x;
  }
  FORCE_INLINE u8& operator[](i32 ix) { return data[ix]; }
  FORCE_INLINE u8 const& operator[](i32 ix) const { return data[ix]; }
};

struct beam_search_config {
  bool print;
  u32  print_interval;
  u64  width;
  f32  features_save_probability;
};

struct beam_search_instance {
  u64* hash_table;
  u32* histogram;

  u32 istep;
  u32 cutoff;
  f32 cutoff_keep_probability;

  u32 low;
  u32 high;

  vector<tuple<i32, features_vec > >* saved_features;
  
  u8 stack_moves[MAX_SOLUTION_SIZE];
  u8 stack_last_move_src[MAX_SOLUTION_SIZE];
  u8 stack_last_move_tgt[MAX_SOLUTION_SIZE];

  void traverse_tour
  (beam_search_config const& config,
   beam_state S,
   euler_tour const& tour_current,
   vector<euler_tour> &tour_nexts
   );
};

struct beam_search_result {
  vector<u8> solution;
  vector<tuple<i32, features_vec > > saved_features;
};

struct beam_search {
  beam_search_config config;
  vector<u64> hash_table;
  vector<u32> histogram;

  beam_search_instance instance;

  beam_search(beam_search_config config_);
  ~beam_search();

  beam_search(beam_search const& other) = delete;
  
  beam_search_result search(beam_state const& initial_state);
};
