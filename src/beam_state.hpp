#pragma once
#include "header.hpp"
#include "puzzle.hpp"

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
  u32 reachability_cost;
  u32 penalty_cost;

  u32 penalty[MAX_SIZE][3][5];

  i32 D[MAX_SIZE*2];
  i32 Q[MAX_SIZE*2];
  
  CUDA_FN
  void reset(puzzle_data const& P,
             puzzle_state const& initial_state,
             u8 initial_direction) {
    src_state = initial_state;
    tgt_state.reset(P);
    last_direction_src = (initial_direction >> 0) & 1;
    last_direction_tgt = (initial_direction >> 1) & 1;

    FOR(i, P.size) FOR(j, 3) FOR(k, 5) penalty[i][j][k] = 0;
    
    total_distance = 0;
    penalty_cost = 0;
    FORU(u, 1, P.size-1) add_dist(P, u);
    reachability_cost = reachability(P);

    hash = get_hash0(P);
  }

  u32 reachability(puzzle_data const& P) {
    return 0;
    return reachability_src(P);
  }
  
  u32 reachability_src(puzzle_data const& P) {
    u32 ret = 0;
    
    FOR(i, P.size) D[i] = P.size;
    i32 qi = MAX_SIZE, qj = MAX_SIZE;
    Q[qj++] = src_state.tok_to_pos[0]; D[src_state.tok_to_pos[0]] = 0;
    while(qi < qj) {
      i32 u = Q[qi++];
      FOR(d, 6) {
        i32 v = P.data[u].data[d];
        i32 cost = D[u] + (src_state.pos_to_tok[v] == tgt_state.pos_to_tok[v]);
        if(D[v] > cost) {
          D[v] = cost;
          if(cost == D[u]) {
            ret = cost;
            Q[--qi] = v;
          }else{
            Q[qj++] = v;
          }
        }
      }
    }

    return ret;
  }
 
  CUDA_FN
  u32 value(puzzle_data const& P) {
    if(penalty_cost) {
      // debug(total_distance, penalty_cost);
    }
    return
      10 * total_distance
      + 16 * penalty_cost
      + 21 * reachability_cost
      + (last_direction_src != last_direction_tgt);
    
    u32 vd = 0;
    FORU(u, 1, P.size-1) {
      vd += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    }

    if(vd != total_distance) {
      debug(vd, total_distance);
    }
    runtime_assert(vd == total_distance);

    u32 vr = reachability(P);
    if(vr != reachability_cost) {
      debug(vr, reachability_cost);
    }
    runtime_assert(vr == reachability_cost);
    
    return vd + vr + (last_direction_src != last_direction_tgt);
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
    auto c = penalty_cost;
    do_move(P, move);
    u32 v = value(P);
    u64 h = get_hash(P);
    undo_move(P, move);
    runtime_assert(c == penalty_cost);
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

      bool redo_reach =
        (b == tgt_state.tok_to_pos[xb]) ||
        (c == tgt_state.tok_to_pos[xc]) ||
        (c == tgt_state.tok_to_pos[xb]) ||
        (a == tgt_state.tok_to_pos[xc]);
      
      rem_dist(P, xb);
      rem_dist(P, xc);
      hash ^= hash_pos(b, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xc]);

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

      if(redo_reach) reachability_cost = reachability(P);
      
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

      bool redo_reach =
        (src_state.tok_to_pos[xb] == b) ||
        (src_state.tok_to_pos[xc] == c) ||
        (src_state.tok_to_pos[xb] == c) ||
        (src_state.tok_to_pos[xc] == a);
 
      rem_dist(P, xb);
      rem_dist(P, xc);     
      hash ^= hash_pos(src_state.tok_to_pos[xb], b);
      hash ^= hash_pos(src_state.tok_to_pos[xc], c);
    
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

      if(redo_reach) reachability_cost = reachability(P);
      
      last_direction_tgt ^= 1;
    }
  }
  
  CUDA_FN
  void undo_move(puzzle_data const& P, u8 move) {
    do_move(P, 6*(move/6) + (move+3)%6);
  }
};
