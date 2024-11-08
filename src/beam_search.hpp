#pragma once
#include "header.hpp"
#include "puzzle.hpp"
#include "eval.hpp"

FORCE_INLINE
u64 hash_hole_src(u32 x) {
  return uint64_hash::hash_int(927648726487624ull + x);
}

FORCE_INLINE
u64 hash_hole_tgt(u32 x) {
  return uint64_hash::hash_int(298749827489724ull + x);
}

FORCE_INLINE
u64 hash_pos(u32 x, u32 y) {
  return uint64_hash::hash_int(x * 4096 + y);
}

struct beam_state {
  puzzle_state src, tgt;

  i32 cost;
  u64 hash;
  
  void init() {
    cost = 0;
    hash = rng.randomInt64();
    
    FORU(u, 1, puzzle.size-1) {
      add_dist(u);
    }
  }

  FORCE_INLINE
  bool is_solved() const {
    return cost == 0 &&
      src.tok_to_pos[0] == tgt.tok_to_pos[0] &&
      src.direction == tgt.direction;
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
  tuple<u32, u64, bool> plan_move(u8 move) {
    do_move(move);
    u32 v = value();
    u64 h = hash;
    bool solved = is_solved();
    undo_move(move);
    return {v,h,solved};
  }
  
  void do_move_src(u8 move, bool forward) {
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

    hash ^= hash_pos(b, tgt.tok_to_pos[xb]);
    hash ^= hash_pos(c, tgt.tok_to_pos[xc]);
    rem_dist(xb);
    rem_dist(xc);

    src.pos_to_tok[a]  = xc;
    src.pos_to_tok[b]  = 0;
    src.pos_to_tok[c]  = xb;
    src.tok_to_pos[0]  = b;
    src.tok_to_pos[xb] = c;
    src.tok_to_pos[xc] = a;

    add_dist(xb);
    add_dist(xc);
    hash ^= hash_pos(c, tgt.tok_to_pos[xb]);
    hash ^= hash_pos(a, tgt.tok_to_pos[xc]);

    src.direction ^= 1;
  }

  void do_move_tgt(u8 move, bool forward){
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

    hash ^= hash_pos(src.tok_to_pos[xb], b);
    hash ^= hash_pos(src.tok_to_pos[xc], c);
    rem_dist(xb);
    rem_dist(xc);    
    
    tgt.pos_to_tok[a]  = xc;
    tgt.pos_to_tok[b]  = 0;
    tgt.pos_to_tok[c]  = xb;
    tgt.tok_to_pos[0]  = b;
    tgt.tok_to_pos[xb] = c;
    tgt.tok_to_pos[xc] = a;

    add_dist(xb);
    add_dist(xc);
    hash ^= hash_pos(src.tok_to_pos[xb], c);
    hash ^= hash_pos(src.tok_to_pos[xc], a);

    tgt.direction ^= 1;
  }
  
  void do_move(u8 move, bool forward = true) {
    if(move < 6) do_move_src(move, forward);
    else do_move_tgt(move - 6, forward);
  }

  void undo_move(u8 move) {
    do_move(6*(move/6) + (move+3)%6, false);
  }

  u32 value() const {
    return cost;
  }

  void print() {
    i32 sz = 1+log10(puzzle.size);
    string spaces = "";
    FOR(i, sz) spaces += ' ';
  
    i32 ix = 0;
    FOR(u, 2*puzzle.n-1) {
      u32 ncol = 2*puzzle.n-1 - abs(u-(puzzle.n-1));
      FOR(i, abs(u-(puzzle.n-1))) cerr << spaces;
      FOR(icol, ncol) {
        auto x = puzzle.tgt_pos_to_tok[tgt.tok_to_pos[src.pos_to_tok[ix]]];
        if(src.pos_to_tok[ix] == tgt.pos_to_tok[ix]) {
          cout << "\033[1;32m" << setw(sz) << x << "\033[0m" << spaces;
        }else{
          cerr << setw(sz) << x << spaces;
        }
        ix += 1;
      }
      cerr << endl;
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

  u32 num_threads;
};

struct beam_search_instance {
  u64* hash_table;
  u32* histogram_heur;

  u32 istep;
  
  u32 cutoff_heur;
  f32 cutoff_heur_keep_probability;

  u32 low_heur;
  u32 high_heur;

  u32 found_solution;

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
};

struct beam_search {
  beam_search_config config;
  vector<u64> hash_table;
  vector<u32> histogram_heur;

  vector<beam_search_instance> L_instances;
  vector<vector<u32>> L_histograms_heur;

  bool should_stop;

  beam_search(beam_search_config config_);
  ~beam_search();

  beam_search(beam_search const& other) = delete;
  
  beam_search_result search(beam_state const& initial_state);
};
