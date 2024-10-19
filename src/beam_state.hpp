#pragma once
#include "header.hpp"
#include "puzzle.hpp"
#include <memory>

const i32 nei_solved_penalty[7]
= {0,2,3,4,5,7,9}; // 847

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
  u32 total_distance;
  u32 penalty_cost;
  u32 nei_cost;
  u32 reachability_error;

  u32  penalty[MAX_SIZE][3][5];
  bool cell_solved[MAX_SIZE];
  u32  nei_solved_count[MAX_SIZE];

  CUDA_FN
  void reset(puzzle_data const& P,
             puzzle_state const& initial_state,
             u8 initial_direction) {
    src_state = initial_state;
    tgt_state.reset(P);
    last_direction_src = (initial_direction >> 0) & 1;
    last_direction_tgt = (initial_direction >> 1) & 1;

    total_distance = 0;
    penalty_cost = 0;
    nei_cost = 0;
    reachability_error = 0;
    
    FOR(i, P.size) FOR(j, 3) FOR(k, 5) penalty[i][j][k] = 0;
    FOR(i, P.size) {
      cell_solved[i] = 0;
      nei_solved_count[i] = 0;
    }
    
    FOR(i, P.size) {
      if(src_state.pos_to_tok[i] != 0 &&
         src_state.pos_to_tok[i] == tgt_state.pos_to_tok[i]) {
        mark_solved(P, i);
      }
    }
    
    FORU(u, 1, P.size-1) add_dist(P, u);

    hash = get_hash0(P);
  }

  // FORCE_INLINE
  // void mark_unsolved(u32 u) {
  //   return;
  //   // cell_solved[u] = 0;
  // }

  // FORCE_INLINE
  // bool try_mark_solved(puzzle_data const& P, u32 u) {
  //   u32 mask = 0;

  //   u32 ncells = 0;
  //   FOR(d, 6) {
  //     i32 x = u;
  //     x = P.data[x].data[d];
  //     if(cell_solved[x]) mask |= bit(ncells);
  //     ncells += 1;
  //     x = P.data[x].data[d];
  //     if(cell_solved[x]) mask |= bit(ncells);
  //     ncells += 1;
  //     x = P.data[x].data[(d+2)%6];
  //     if(cell_solved[x]) mask |= bit(ncells);
  //     ncells += 1;
  //   }
    
  //   if(reachability.data[mask]) {
  //     cell_solved[u] = 1;
  //     return true;
  //   }else{
  //     return false;
  //   }
  // }

  // FORCE_INLINE
  // void try_mark_solved2(puzzle_data const& P,
  //                       u32 c, bool doc,
  //                       u32 a, bool doa) {
  //   return;
  //   // if(doc && doa) {
  //   //   if(try_mark_solved(P, c)) {
  //   //     reachability_error = !try_mark_solved(P, a);
  //   //   }if(try_mark_solved(P, a)) {
  //   //     reachability_error = !try_mark_solved(P, c);
  //   //   }else{
  //   //     reachability_error = 1;
  //   //   }
  //   // }else if(doc) {
  //   //   reachability_error = !try_mark_solved(P, c);
  //   // }else if(doa) {
  //   //   reachability_error = !try_mark_solved(P, a);
  //   // }else{
  //   //   reachability_error = 0;
  //   // }
    
  // }

  void mark_unsolved(puzzle_data const& P, u32 u) {
    runtime_assert(cell_solved[u]);
    cell_solved[u] = 0;
    nei_cost += nei_solved_penalty[nei_solved_count[u]];
    FOR(d, 6) {
      auto v = P.data[u].data[d];
      if(!cell_solved[v]) {
        nei_cost -= nei_solved_penalty[nei_solved_count[v]];
        nei_cost += nei_solved_penalty[nei_solved_count[v]-1];
      }
      nei_solved_count[v] -= 1;
    }
  }

  void mark_solved(puzzle_data const& P, u32 u) {
    runtime_assert(!cell_solved[u]);
    cell_solved[u] = 1;
    nei_cost -= nei_solved_penalty[nei_solved_count[u]];
    FOR(d, 6) {
      auto v = P.data[u].data[d];
      if(!cell_solved[v]) {
        nei_cost -= nei_solved_penalty[nei_solved_count[v]];
        nei_cost += nei_solved_penalty[nei_solved_count[v]+1];
      }
      nei_solved_count[v] += 1;
    }
  }
  
  CUDA_FN
  u32 value(puzzle_data const& P) {

    // i32 true_nei_cost = 0;
    // FOR(u, P.size) if(!cell_solved[u]) {
    //   i32 cnt = 0;
    //   FOR(d, 6) {
    //     auto v = P.data[u].data[d];
    //     if(cell_solved[v]) cnt += 1;
    //   }
    //   runtime_assert(cnt == nei_solved_count[u]);
    //   true_nei_cost += nei_solved_penalty[cnt];
    // }
    // // debug(nei_cost, true_nei_cost);
    // runtime_assert(nei_cost == true_nei_cost);
    
    return
      10 * total_distance
      + 8 * penalty_cost
      + 1 * (nei_cost - nei_solved_penalty[6])
      + (last_direction_src != last_direction_tgt);
    
    // u32 vd = 0;
    // FORU(u, 1, P.size-1) {
    //   vd += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    // }

    // if(vd != total_distance) {
    //   debug(vd, total_distance);
    // }
    // runtime_assert(vd == total_distance);
   
    // return vd + (last_direction_src != last_direction_tgt);
  }
  
  CUDA_FN
  u64 get_hash0(puzzle_data const& P) const {
    u64 h = 0;
    FORU(u, 1, P.size-1) h ^= hash_pos(src_state.tok_to_pos[u], tgt_state.tok_to_pos[u]);
    return h;
  }

  CUDA_FN
  u64 get_hash(puzzle_data const& P) const {
    return hash;
  }

  FORCE_INLINE
  void add_dist(puzzle_data const& P, u32 u) {
    u32 x = src_state.tok_to_pos[u];
    u32 y = tgt_state.tok_to_pos[u];
    total_distance += P.dist[x][y];
    auto dir = P.dist_direction[x][y];
    if(dir[0] == 3) return;
    if(dir[1] == 0) {
      penalty[x][0][1] += 1;
      penalty[x][1][1] += 1;
      penalty[x][2][1] += 1;
      penalty_cost += penalty[x][0][3];
      penalty_cost += penalty[x][0][4];
      penalty_cost += penalty[x][1][3];
      penalty_cost += penalty[x][1][4];
      penalty_cost += penalty[x][2][3];
      penalty_cost += penalty[x][2][4];
    }else if(dir[1] < 0) {
      penalty[x][dir[0]][0] += 1;
      penalty_cost += penalty[x][dir[0]][4];
      auto z = x;
      FORD(i, -1, dir[1]) {
        z = P.data[z].data[dir[0]+3];
        penalty[z][dir[0]][3] += 1;
        penalty_cost += penalty[z][dir[0]][1];
        penalty_cost += penalty[z][dir[0]][2];
      }
    }else if(dir[1] > 0) {
      penalty[x][dir[0]][2] += 1;
      penalty_cost += penalty[x][dir[0]][3];
      auto z = x;
      FORU(i, 1, dir[1]) {
        z = P.data[z].data[dir[0]];
        penalty[z][dir[0]][4] += 1;
        penalty_cost += penalty[z][dir[0]][0];
        penalty_cost += penalty[z][dir[0]][1];
      }
    }
  }
 
  FORCE_INLINE
  void rem_dist(puzzle_data const& P, u32 u) {
    u32 x = src_state.tok_to_pos[u];
    u32 y = tgt_state.tok_to_pos[u];
    total_distance -= P.dist[x][y];
    auto dir = P.dist_direction[x][y];
    if(dir[0] == 3) return;
    if(dir[1] == 0) {
      penalty[x][0][1] -= 1;
      penalty[x][1][1] -= 1;
      penalty[x][2][1] -= 1;
      penalty_cost -= penalty[x][0][3];
      penalty_cost -= penalty[x][0][4];
      penalty_cost -= penalty[x][1][3];
      penalty_cost -= penalty[x][1][4];
      penalty_cost -= penalty[x][2][3];
      penalty_cost -= penalty[x][2][4];
    }else if(dir[1] < 0) {
      penalty[x][dir[0]][0] -= 1;
      penalty_cost -= penalty[x][dir[0]][4];
      auto z = x;
      FORD(i, -1, dir[1]) {
        z = P.data[z].data[dir[0]+3];
        penalty[z][dir[0]][3] -= 1;
        penalty_cost -= penalty[z][dir[0]][1];
        penalty_cost -= penalty[z][dir[0]][2];
      }
    }else if(dir[1] > 0) {
      penalty[x][dir[0]][2] -= 1;
      penalty_cost -= penalty[x][dir[0]][3];
      auto z = x;
      FORU(i, 1, dir[1]) {
        z = P.data[z].data[dir[0]];
        penalty[z][dir[0]][4] -= 1;
        penalty_cost -= penalty[z][dir[0]][0];
        penalty_cost -= penalty[z][dir[0]][1];
      }
    }
  }
  
  CUDA_FN
  tuple<u32, u64> plan_move(puzzle_data const& P, u8 move) {
    auto old_nei = nei_cost;
    do_move(P, move);
    u32 v = value(P);
    u64 h = get_hash(P);
    undo_move(P, move);
    runtime_assert(nei_cost == old_nei);
    return {v,h};
  }
    
  CUDA_FN
  void do_move(puzzle_data const& P, u8 move) {
    if(move < 6) {
      u32 a = src_state.tok_to_pos[0], b, c;
    
      if(last_direction_src == 0) {
        b = P.data[a].data[move];
        c = P.data[a].data[(move+1)%6];
      }else{
        b = P.data[a].data[move];
        c = P.data[a].data[(move+5)%6];
      }

      u32 xb = src_state.pos_to_tok[b];
      u32 xc = src_state.pos_to_tok[c];

      rem_dist(P, xb);
      rem_dist(P, xc);
      hash ^= hash_pos(b, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xc]);
      if(b == tgt_state.tok_to_pos[xb]) mark_unsolved(P, b);
      if(c == tgt_state.tok_to_pos[xc]) mark_unsolved(P, c);

      src_state.pos_to_tok[a]  = xc;
      src_state.pos_to_tok[b]  = 0;
      src_state.pos_to_tok[c]  = xb;
      src_state.tok_to_pos[0]  = b;
      src_state.tok_to_pos[xb] = c;
      src_state.tok_to_pos[xc] = a;

      add_dist(P, xc);
      add_dist(P, xb);
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(a, tgt_state.tok_to_pos[xc]);
      if(c == tgt_state.tok_to_pos[xb]) mark_solved(P, c);
      if(a == tgt_state.tok_to_pos[xc]) mark_solved(P, a);

      last_direction_src ^= 1;

    }else{
      u32 a = tgt_state.tok_to_pos[0], b, c;
      move -= 6;
      
      if(last_direction_tgt == 0) {
        b = P.data[a].data[move];
        c = P.data[a].data[(move+1)%6];
      }else{
        b = P.data[a].data[move];
        c = P.data[a].data[(move+5)%6];
      }

      u32 xb = tgt_state.pos_to_tok[b];
      u32 xc = tgt_state.pos_to_tok[c];

      rem_dist(P, xb);
      rem_dist(P, xc);     
      hash ^= hash_pos(src_state.tok_to_pos[xb], b);
      hash ^= hash_pos(src_state.tok_to_pos[xc], c);
      if(src_state.tok_to_pos[xb] == b) mark_unsolved(P, b);
      if(src_state.tok_to_pos[xc] == c) mark_unsolved(P, c);
    
      tgt_state.pos_to_tok[a]  = xc;
      tgt_state.pos_to_tok[b]  = 0;
      tgt_state.pos_to_tok[c]  = xb;
      tgt_state.tok_to_pos[0]  = b;
      tgt_state.tok_to_pos[xb] = c;
      tgt_state.tok_to_pos[xc] = a;

      add_dist(P, xc);
      add_dist(P, xb);
      hash ^= hash_pos(src_state.tok_to_pos[xb], c);
      hash ^= hash_pos(src_state.tok_to_pos[xc], a);
      if(src_state.tok_to_pos[xb] == c) mark_solved(P, c);
      if(src_state.tok_to_pos[xc] == a) mark_solved(P, a);

      last_direction_tgt ^= 1;
    }
  }
  
  CUDA_FN
  void undo_move(puzzle_data const& P, u8 move) {
    do_move(P, 6*(move/6) + (move+3)%6);
  }

  void print(puzzle_data const& P) {
    u32 ix = 0;
    FOR(u, 2*P.n-1) {
      u32 ncol = 2*P.n-1 - abs(u-(P.n-1));
      FOR(i, abs(u-(P.n-1))) cerr << "    ";
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
