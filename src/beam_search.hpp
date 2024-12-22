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

  cost_t cost;
  u64 hash;
  
  u32  num_unsolved;
  bool cell_solved[MAX_SIZE];
  u16  cell_nei_solved[MAX_SIZE];
  
  void init() {
    cost.reset();
    hash = rng.randomInt64();

    num_unsolved = puzzle.size-1;
    cost.rem_nei(bit(6)-1);

    FOR(u, puzzle.size) {
      cell_solved[u] = 0;
      cell_nei_solved[u] = 0;
    }

    FOR(u, puzzle.size) if(src.pos_to_tok[u] != 0 &&
                           src.pos_to_tok[u] == tgt.pos_to_tok[u]) {
      add_solved(u);
    }
    
    FORU(u, 1, puzzle.size-1) {
      add_dist(u);
    }
  }

  FORCE_INLINE
  bool is_solved() const {
    return num_unsolved == 0 &&
      src.direction == tgt.direction;
  }

  void add_dist(u32 u) {
    u32 x = src.tok_to_pos[u];
    u32 y = tgt.tok_to_pos[u];
    cost.add_dist(x,y);
  }

  FORCE_INLINE
  void add_solved(u32 u) {
    num_unsolved -= 1;
    cell_solved[u] = 1;
    cost.rem_nei(cell_nei_solved[u]);
    FOR(d, 6) {
      auto v = puzzle.rot[u][d];
      if(!cell_solved[v]) {
        cost.rem_nei(cell_nei_solved[v]);
        cell_nei_solved[v] ^= bit(d);
        cost.add_nei(cell_nei_solved[v]);
      }else{
        cell_nei_solved[v] ^= bit(d);
      }
    }
  }
 
  FORCE_INLINE
  void rem_solved(u32 u) {
    num_unsolved += 1;
    cell_solved[u] = 0;
    FOR(d, 6) {
      auto v = puzzle.rot[u][d];
      if(!cell_solved[v]) {
        cost.rem_nei(cell_nei_solved[v]);
        cell_nei_solved[v] ^= bit(d);
        cost.add_nei(cell_nei_solved[v]);
      }else{
        cell_nei_solved[v] ^= bit(d);
      }
    }
    cost.add_nei(cell_nei_solved[u]);
  }
  
  // FORCE_INLINE
  // tuple<u32, u64, bool> plan_move_src(u8 move) const {
  //   u32 v = cost;
  //   u64 h = hash;
    
  //   u32 a = src.tok_to_pos[0], b, c;
  //   b = puzzle.rot[a][move];
  //   c = puzzle.rot[a][(move+(src.direction?5:1))%6];

  //   u32 xb = src.pos_to_tok[b];
  //   u32 xc = src.pos_to_tok[c];

  //   h ^= hash_pos(b, tgt.tok_to_pos[xb]);
  //   h ^= hash_pos(c, tgt.tok_to_pos[xc]);
  //   v -= weights.dist_weight[b][tgt.tok_to_pos[xb]];
  //   v -= weights.dist_weight[c][tgt.tok_to_pos[xc]];

  //   v += weights.dist_weight[c][tgt.tok_to_pos[xb]];
  //   v += weights.dist_weight[a][tgt.tok_to_pos[xc]];
  //   h ^= hash_pos(c, tgt.tok_to_pos[xb]);
  //   h ^= hash_pos(a, tgt.tok_to_pos[xc]);

  //   bool solved = v == 0 && src.direction != tgt.direction;
    
  //   return {v,h,solved};
  // }
 
  // FORCE_INLINE
  // tuple<u32, u64, bool> plan_move_tgt(u8 move) const {
  //   u32 v = cost;
  //   u64 h = hash;

  //   u32 a = tgt.tok_to_pos[0], b, c;
  //   b = puzzle.rot[a][move];
  //   c = puzzle.rot[a][(move+(tgt.direction?5:1))%6];

  //   u32 xb = tgt.pos_to_tok[b];
  //   u32 xc = tgt.pos_to_tok[c];

  //   h ^= hash_pos(src.tok_to_pos[xb], b);
  //   h ^= hash_pos(src.tok_to_pos[xc], c);
  //   v -= weights.dist_weight[src.tok_to_pos[xb]][b];
  //   v -= weights.dist_weight[src.tok_to_pos[xc]][c];
    
  //   v += weights.dist_weight[src.tok_to_pos[xb]][c];
  //   v += weights.dist_weight[src.tok_to_pos[xc]][a];   
  //   h ^= hash_pos(src.tok_to_pos[xb], c);
  //   h ^= hash_pos(src.tok_to_pos[xc], a);

  //   bool solved = v == 0 && src.direction != tgt.direction;
    
  //   return {v,h,solved};
  // }
 
  FORCE_INLINE
  tuple<u32, u64, bool> plan_move(u8 move) {
    do_move(move);
    auto v = value();
    auto h = hash;
    auto s = is_solved();
    undo_move(move);
    return {v,h,s};
    
    // if(move < 6) return plan_move_src(move);
    // else return plan_move_tgt(move - 6);
  }

  FORCE_INLINE
  void do_move_src(u8 move) {
    u32 a = src.tok_to_pos[0], b, c;
    b = puzzle.rot[a][move];
    c = puzzle.rot[a][(move+(src.direction?5:1))%6];

    u32 xb = src.pos_to_tok[b];
    u32 xc = src.pos_to_tok[c];

    hash ^= hash_pos(b, tgt.tok_to_pos[xb]);
    hash ^= hash_pos(c, tgt.tok_to_pos[xc]);
    cost.rem_dist(b, tgt.tok_to_pos[xb]);
    cost.rem_dist(c, tgt.tok_to_pos[xc]);
    if(b == tgt.tok_to_pos[xb]) rem_solved(b);
    if(c == tgt.tok_to_pos[xc]) rem_solved(c);

    src.pos_to_tok[a]  = xc;
    src.pos_to_tok[b]  = 0;
    src.pos_to_tok[c]  = xb;
    src.tok_to_pos[0]  = b;
    src.tok_to_pos[xb] = c;
    src.tok_to_pos[xc] = a;

    if(c == tgt.tok_to_pos[xb]) add_solved(c);
    if(a == tgt.tok_to_pos[xc]) add_solved(a);
    cost.add_dist(c, tgt.tok_to_pos[xb]);
    cost.add_dist(a, tgt.tok_to_pos[xc]);
    hash ^= hash_pos(c, tgt.tok_to_pos[xb]);
    hash ^= hash_pos(a, tgt.tok_to_pos[xc]);

    src.direction ^= 1;
  }

  FORCE_INLINE
  void do_move_tgt(u8 move){
    u32 a = tgt.tok_to_pos[0], b, c;
    b = puzzle.rot[a][move];
    c = puzzle.rot[a][(move+(tgt.direction?5:1))%6];

    u32 xb = tgt.pos_to_tok[b];
    u32 xc = tgt.pos_to_tok[c];

    hash ^= hash_pos(src.tok_to_pos[xb], b);
    hash ^= hash_pos(src.tok_to_pos[xc], c);
    cost.rem_dist(src.tok_to_pos[xb], b);
    cost.rem_dist(src.tok_to_pos[xc], c);
    if(src.tok_to_pos[xb] == b) rem_solved(b);
    if(src.tok_to_pos[xc] == c) rem_solved(c);
    
    tgt.pos_to_tok[a]  = xc;
    tgt.pos_to_tok[b]  = 0;
    tgt.pos_to_tok[c]  = xb;
    tgt.tok_to_pos[0]  = b;
    tgt.tok_to_pos[xb] = c;
    tgt.tok_to_pos[xc] = a;
    
    if(src.tok_to_pos[xb] == c) add_solved(c);
    if(src.tok_to_pos[xc] == a) add_solved(a);
    cost.add_dist(src.tok_to_pos[xb], c);
    cost.add_dist(src.tok_to_pos[xc], a);
    hash ^= hash_pos(src.tok_to_pos[xb], c);
    hash ^= hash_pos(src.tok_to_pos[xc], a);

    tgt.direction ^= 1;
  }
  
  FORCE_INLINE
  void do_move(u8 move) {
    if(move < 6) do_move_src(move);
    else do_move_tgt(move - 6);
  }

  FORCE_INLINE
  void undo_move(u8 move) {
    do_move(6*(move/6) + (move+3)%6);
  }

  FORCE_INLINE
  i32 value() const {
    return cost.eval();
  }

  void features(features_vec& V) const {
    FOR(i, NUM_FEATURES) V[i] = 0;
    FORU(u, 1, puzzle.size-1) {
      u32 x = src.tok_to_pos[u];
      u32 y = tgt.tok_to_pos[u];
      auto p = puzzle.dist_pair[x][y];
      V[dist_feature_key[p[0]][p[1]]] += 1;
    }
    FOR(u, puzzle.size) {
      if(!cell_solved[u]) {
        V[nei_feature_key[cell_nei_solved[u]]] += 1;
      }
    }
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
  f32  features_save_probability;
  
  u32  num_threads;
};

struct beam_search_instance {
  u64* hash_table;
  u32* histogram_heur;

  u32 istep;
  
  i32 cutoff_heur;
  f32 cutoff_heur_keep_probability;

  u32 low_heur;
  u32 high_heur;

  u32 found_solution;

  u8 stack_moves[MAX_SOLUTION_SIZE];
  u8 stack_last_move_src[MAX_SOLUTION_SIZE];
  u8 stack_last_move_tgt[MAX_SOLUTION_SIZE];

  vector<tuple<i32, features_vec > >* saved_features;

  void traverse_tour
  (beam_search_config const& config,
   beam_state S,
   euler_tour const& tour_current,
   vector<euler_tour> &tour_nexts
   );
};

struct beam_search_result_entry {
  i32 step;
  u32 min_cost;
  f32 avg_cost;
};

struct beam_search_result {
  vector<u8> solution;
  vector<tuple<i32, features_vec > > saved_features;

  vector<beam_search_result_entry> graph;
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
