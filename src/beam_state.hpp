#pragma once
#include "header.hpp"
#include "puzzle.hpp"
#include "evaluation.hpp"

// const i32 nei_solved_penalty[7]
// = {0,0,2,4,7,10,14};

struct reachability_t {
  bitset<1<<18> data;

  void init() {
    unique_ptr<puzzle_data> P9 = make_unique<puzzle_data>();
    P9->make(9);

    u32 cells[19]; i32 ncells = 0;

    vector<u32> G[19];
    
    FOR(d, 6) {
      i32 x = P9->center;
      x = P9->data[x].data[d];
      cells[ncells++] = x;
      x = P9->data[x].data[d];
      cells[ncells++] = x;
      x = P9->data[x].data[(d+2)%6];
      cells[ncells++] = x;
    }
    cells[ncells++] = P9->center;

    runtime_assert(ncells == 19);
    debug(vector<i32>(cells, cells+19));

    FOR(i, 19) FOR(d, 6) FOR(j, 19) if(cells[j] == P9->data[cells[i]].data[d]) {
      G[i].pb(j);
    }
    
    FOR(mask, 1<<19) if(mask & bit(18)) {
      i32 uf[19];
      FOR(i, 19) uf[i] = i;
      auto find = [&](auto self, i32 i) -> i32 {
        return uf[i] == i ? i : uf[i] = self(self, uf[i]);
      };
      auto unite = [&](i32 i, i32 j) {
        i = find(find,i);
        j = find(find,j);
        if(i != j) uf[i] = j;
      };
      
      FOR(i, 19) if(!(mask&bit(i))) for(auto j : G[i]) if(!(mask&bit(j))) {
            unite(i,j);
          }

      i32 nroots = 0;
      FOR(i, 19) if(!(mask&bit(i))) {
        if(find(find, i) == i) nroots += 1;
      }
      data[mask & (bit(18)-1)] = (nroots < 2);

      // bool pr[19];
      // FOR(i, 19) pr[i] = mask&bit(i);
      // if(!(data[mask & (bit(18)-1)])) {
      //   cout << "==========" << endl;
      //   cout << "  " << pr[13] << " " << pr[14] << " " << pr[16] << endl;
      //   cout << " " << pr[11] << " " << pr[12] << " " << pr[15] << " " << pr[17] << endl;
      //   cout << pr[10] << " " << pr[9] << " " << pr[18] << " " << pr[0] << " " << pr[1] << endl;
      //   cout << " " << pr[8] << " " << pr[6] << " " << pr[3] << " " << pr[2] << endl;
      //   cout << "  " << pr[7] << " " << pr[5] << " " << pr[4] << endl;
      // }
    }
  }
};

extern reachability_t reachability;

FORCE_INLINE
u64 hash_pos(u32 x, u32 y) {
  return uint64_hash::hash_int(x * 4096 + y);
}

struct beam_state {
  puzzle_state src_state;
  puzzle_state tgt_state;
  u8 last_direction_src;
  u8 last_direction_tgt;

  u64 hash;
  i32 cost;

  bool cell_solved[MAX_SIZE];
  u32  nei_solved_count[MAX_SIZE];

  CUDA_FN
  void reset(puzzle_data const& puzzle,
             weights_t const& W,
             puzzle_state const& initial_state,
             u8 initial_direction) {

    src_state = initial_state;
    tgt_state.reset(puzzle);
    last_direction_src = (initial_direction >> 0) & 1;
    last_direction_tgt = (initial_direction >> 1) & 1;

    reinit(puzzle, W);
  }
 
  CUDA_FN
  void reinit(puzzle_data const& puzzle,
              weights_t const& W) {
    cost = 0;
    cost -= W.nei_weights[6];
    
    FOR(i, puzzle.size) {
      cell_solved[i] = 0;
      nei_solved_count[i] = 0;
    }
    
    FOR(i, puzzle.size) {
      if(src_state.pos_to_tok[i] != 0 &&
         src_state.pos_to_tok[i] == tgt_state.pos_to_tok[i]) {
        mark_solved(puzzle, W, i);
      }
    }
    
    FORU(u, 1, puzzle.size-1) add_dist(puzzle, W, u);

    FORU(u, 1, puzzle.size-1) {
      hash ^= hash_pos(src_state.tok_to_pos[u], tgt_state.tok_to_pos[u]);
    }
  }

  void mark_unsolved(puzzle_data const& puzzle, weights_t const& W, u32 u) {
    runtime_assert(cell_solved[u]);
    cell_solved[u] = 0;
    cost += W.nei_weights[nei_solved_count[u]];
    FOR(d, 6) {
      auto v = puzzle.data[u].data[d];
      if(!cell_solved[v]) {
        cost -= W.nei_weights[nei_solved_count[v]];
        cost += W.nei_weights[nei_solved_count[v]-1];
      }
      nei_solved_count[v] -= 1;
    }
  }

  void mark_solved(puzzle_data const& puzzle, weights_t const& W, u32 u) {
    runtime_assert(!cell_solved[u]);
    cell_solved[u] = 1;
    cost -= W.nei_weights[nei_solved_count[u]];
    FOR(d, 6) {
      auto v = puzzle.data[u].data[d];
      if(!cell_solved[v]) {
        cost -= W.nei_weights[nei_solved_count[v]];
        cost += W.nei_weights[nei_solved_count[v]+1];
      }
      nei_solved_count[v] += 1;
    }
  }
  
  CUDA_FN
  u32 value() const {
    return
      cost
      + (last_direction_src != last_direction_tgt);
  }
  
  FORCE_INLINE
  void add_dist(puzzle_data const& puzzle, weights_t const& W, u32 u) {
    u32 x = src_state.tok_to_pos[u];
    u32 y = tgt_state.tok_to_pos[u];
    cost += W.dist_weights[puzzle.dist_key[x][y]];
  }
 
  FORCE_INLINE
  void rem_dist(puzzle_data const& puzzle, weights_t const& W, u32 u) {
    u32 x = src_state.tok_to_pos[u];
    u32 y = tgt_state.tok_to_pos[u];
    cost -= W.dist_weights[puzzle.dist_key[x][y]];
  }
  
  CUDA_FN
  tuple<u32, u64> plan_move(puzzle_data const& puzzle, weights_t const& W, u8 move) {
    do_move(puzzle, W, move);
    u32 v = value();
    u64 h = hash;
    undo_move(puzzle, W, move);
    return {v,h};
  }
    
  CUDA_FN
  void do_move(puzzle_data const& puzzle, weights_t const& W, u8 move) {
    if(move < 6) {
      u32 a = src_state.tok_to_pos[0], b, c;
    
      if(last_direction_src == 0) {
        b = puzzle.data[a].data[move];
        c = puzzle.data[a].data[(move+1)%6];
      }else{
        b = puzzle.data[a].data[move];
        c = puzzle.data[a].data[(move+5)%6];
      }

      u32 xb = src_state.pos_to_tok[b];
      u32 xc = src_state.pos_to_tok[c];

      rem_dist(puzzle, W, xb);
      rem_dist(puzzle, W, xc);
      hash ^= hash_pos(b, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xc]);
      if(b == tgt_state.tok_to_pos[xb]) mark_unsolved(puzzle, W, b);
      if(c == tgt_state.tok_to_pos[xc]) mark_unsolved(puzzle, W, c);

      src_state.pos_to_tok[a]  = xc;
      src_state.pos_to_tok[b]  = 0;
      src_state.pos_to_tok[c]  = xb;
      src_state.tok_to_pos[0]  = b;
      src_state.tok_to_pos[xb] = c;
      src_state.tok_to_pos[xc] = a;

      add_dist(puzzle, W, xc);
      add_dist(puzzle, W, xb);
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(a, tgt_state.tok_to_pos[xc]);
      if(c == tgt_state.tok_to_pos[xb]) mark_solved(puzzle, W, c);
      if(a == tgt_state.tok_to_pos[xc]) mark_solved(puzzle, W, a);

      last_direction_src ^= 1;

    }else{
      u32 a = tgt_state.tok_to_pos[0], b, c;
      move -= 6;
      
      if(last_direction_tgt == 0) {
        b = puzzle.data[a].data[move];
        c = puzzle.data[a].data[(move+1)%6];
      }else{
        b = puzzle.data[a].data[move];
        c = puzzle.data[a].data[(move+5)%6];
      }

      u32 xb = tgt_state.pos_to_tok[b];
      u32 xc = tgt_state.pos_to_tok[c];

      rem_dist(puzzle, W, xb);
      rem_dist(puzzle, W, xc);     
      hash ^= hash_pos(src_state.tok_to_pos[xb], b);
      hash ^= hash_pos(src_state.tok_to_pos[xc], c);
      if(src_state.tok_to_pos[xb] == b) mark_unsolved(puzzle, W, b);
      if(src_state.tok_to_pos[xc] == c) mark_unsolved(puzzle, W, c);
    
      tgt_state.pos_to_tok[a]  = xc;
      tgt_state.pos_to_tok[b]  = 0;
      tgt_state.pos_to_tok[c]  = xb;
      tgt_state.tok_to_pos[0]  = b;
      tgt_state.tok_to_pos[xb] = c;
      tgt_state.tok_to_pos[xc] = a;

      add_dist(puzzle, W, xc);
      add_dist(puzzle, W, xb);
      hash ^= hash_pos(src_state.tok_to_pos[xb], c);
      hash ^= hash_pos(src_state.tok_to_pos[xc], a);
      if(src_state.tok_to_pos[xb] == c) mark_solved(puzzle, W, c);
      if(src_state.tok_to_pos[xc] == a) mark_solved(puzzle, W, a);

      last_direction_tgt ^= 1;
    }
  }
  
  CUDA_FN
  void undo_move(puzzle_data const& puzzle, weights_t const& W, u8 move) {
    do_move(puzzle, W, 6*(move/6) + (move+3)%6);
  }

  void print(puzzle_data const& puzzle) {
    u32 ix = 0;
    FOR(u, 2*puzzle.n-1) {
      u32 ncol = 2*puzzle.n-1 - abs(u-(puzzle.n-1));
      FOR(i, abs(u-(puzzle.n-1))) cerr << "    ";
      FOR(icol, ncol) {
        u32 u = src_state.pos_to_tok[ix];
        if(u == 0) {
        }else{
          u = tgt_state.tok_to_pos[u];
          if(u < tgt_state.tok_to_pos[0]) u += 1;
        }
        if(ix == tgt_state.tok_to_pos[0]) {
          cout << "\033[1;31m" << setw(4) << u << "\033[0m" << "    ";
        }else if(src_state.pos_to_tok[ix] == tgt_state.pos_to_tok[ix]) {
          cout << "\033[1;32m" << setw(4) << u << "\033[0m" << "    ";
        }else{
          cout << setw(4) << u << "    ";
        }
        ix += 1;
      }
      cerr << endl;
    }
  }
};
