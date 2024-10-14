#pragma once
#include "base.hpp"

static inline
CUDA_FN
u64 hash_pos(u32 x, u32 y) {
  return uint64_hash::hash_int(x * 4096 + y);
}

struct beam_state {
  puzzle_state src_state;
  puzzle_state tgt_state;
  u8 last_direction_src;
  u8 last_direction_tgt;

  i64 total_distance;
  u64 hash;

  CUDA_FN
  void reset(puzzle_data const& P,
             puzzle_state const& initial_state,
             u8 initial_direction) {
    src_state = initial_state;
    tgt_state.reset(P);
    last_direction_src = (initial_direction >> 0) & 1;
    last_direction_tgt = (initial_direction >> 1) & 1;

    total_distance = 0;
    FORU(u, 1, P.size-1) {
      total_distance += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    }
    FORU(u, 1, P.size-1) if(loop2_token(u)) total_distance += 1;

    hash = get_hash0(P);
  }

  CUDA_FN
  bool loop2_token(u32 u) const {
    u32 pos1 = src_state.tok_to_pos[u], pos2 = tgt_state.tok_to_pos[u];
    if(pos1 != pos2 &&
       src_state.pos_to_tok[pos2] != 0 &&
       src_state.pos_to_tok[pos2] == tgt_state.pos_to_tok[pos1]) {
      return 1;
    }
    return 0;
  }
  
  CUDA_FN
  i64 value(puzzle_data const& P) const {
    return total_distance + (last_direction_src != last_direction_tgt);
    
//     i64 v = 0;
//     FORU(u, 1, P.size-1) {
//       v += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
//     }
//     FORU(u, 1, P.size-1) if(loop2_token(u)) v += 1;

// #ifndef __CUDA_ARCH__
//     if(v != total_distance) {
//       debug(v, total_distance);
//     }
//     runtime_assert(v == total_distance);
// #endif
//     return v + (last_direction_src != last_direction_tgt);
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
  
  CUDA_FN
  tuple<i64, u64> plan_move(puzzle_data const& P, u8 move) {
    i64 v = total_distance; u64 h = hash;
    
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
      
      v -= P.dist[b][tgt_state.tok_to_pos[xb]];
      v -= P.dist[c][tgt_state.tok_to_pos[xc]];
      if(loop2_token(xb)) v -= 2;
      if(src_state.pos_to_tok[tgt_state.tok_to_pos[xb]] != xc &&
         loop2_token(xc)) v -= 2;
      h ^= hash_pos(b, tgt_state.tok_to_pos[xb]);
      h ^= hash_pos(c, tgt_state.tok_to_pos[xc]);

      v += P.dist[c][tgt_state.tok_to_pos[xb]];
      v += P.dist[a][tgt_state.tok_to_pos[xc]];

      { u32 pos2 = tgt_state.tok_to_pos[xb];
        if(pos2 != c &&
           pos2 != b &&
           ((pos2 != a && src_state.pos_to_tok[pos2] == tgt_state.pos_to_tok[c]) ||
            (pos2 == a && xc == tgt_state.pos_to_tok[c]))) {
          v += 2;
        }
      }
      if(tgt_state.tok_to_pos[xb] != a) {
        u32 pos2 = tgt_state.tok_to_pos[xc];
        if(pos2 != a &&
           pos2 != b &&
           ((pos2 != a && src_state.pos_to_tok[pos2] == tgt_state.pos_to_tok[a]) ||
            (pos2 == a && xc == tgt_state.pos_to_tok[a]))) {
          v += 2;
        }
      }

      h ^= hash_pos(c, tgt_state.tok_to_pos[xb]);
      h ^= hash_pos(a, tgt_state.tok_to_pos[xc]);
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

      v -= P.dist[src_state.tok_to_pos[xb]][b];
      v -= P.dist[src_state.tok_to_pos[xc]][c];
      if(loop2_token(xb)) v -= 2;
      if(src_state.pos_to_tok[tgt_state.tok_to_pos[xb]] != xc &&
         loop2_token(xc)) v -= 2;
      h ^= hash_pos(src_state.tok_to_pos[xb], b);
      h ^= hash_pos(src_state.tok_to_pos[xc], c);

      v += P.dist[src_state.tok_to_pos[xb]][c];
      v += P.dist[src_state.tok_to_pos[xc]][a];

      { u32 pos2 = src_state.tok_to_pos[xb];
        if(pos2 != c &&
           pos2 != b &&
           ((pos2 != a && tgt_state.pos_to_tok[pos2] == src_state.pos_to_tok[c]) ||
            (pos2 == a && xc == src_state.pos_to_tok[c]))) {
          v += 2;
        }
      }
      if(src_state.tok_to_pos[xb] != a) {
        u32 pos2 = src_state.tok_to_pos[xc];
        if(pos2 != a &&
           pos2 != b &&
           ((pos2 != a && tgt_state.pos_to_tok[pos2] == src_state.pos_to_tok[a]) ||
            (pos2 == a && xc == src_state.pos_to_tok[a]))) {
          v += 2;
        }
      }

      h ^= hash_pos(src_state.tok_to_pos[xb], c);
      h ^= hash_pos(src_state.tok_to_pos[xc], a);
    }

    return mt(v + (last_direction_src == last_direction_tgt), h);
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

      total_distance -= P.dist[b][tgt_state.tok_to_pos[xb]];
      total_distance -= P.dist[c][tgt_state.tok_to_pos[xc]];
      if(loop2_token(xb)) total_distance -= 2;
      if(src_state.pos_to_tok[tgt_state.tok_to_pos[xb]] != xc &&
         loop2_token(xc)) total_distance -= 2;
      hash ^= hash_pos(b, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xc]);

      src_state.pos_to_tok[a]  = xc;
      src_state.pos_to_tok[b]  = 0;
      src_state.pos_to_tok[c]  = xb;
      src_state.tok_to_pos[0]  = b;
      src_state.tok_to_pos[xb] = c;
      src_state.tok_to_pos[xc] = a;

      total_distance += P.dist[c][tgt_state.tok_to_pos[xb]];
      total_distance += P.dist[a][tgt_state.tok_to_pos[xc]];
      hash ^= hash_pos(c, tgt_state.tok_to_pos[xb]);
      hash ^= hash_pos(a, tgt_state.tok_to_pos[xc]);
      if(loop2_token(xb)) total_distance += 2;
      if(src_state.pos_to_tok[tgt_state.tok_to_pos[xb]] != xc &&
         loop2_token(xc)) total_distance += 2;

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

      total_distance -= P.dist[src_state.tok_to_pos[xb]][b];
      total_distance -= P.dist[src_state.tok_to_pos[xc]][c];
      if(loop2_token(xb)) total_distance -= 2;
      if(src_state.pos_to_tok[tgt_state.tok_to_pos[xb]] != xc &&
         loop2_token(xc)) total_distance -= 2;
      hash ^= hash_pos(src_state.tok_to_pos[xb], b);
      hash ^= hash_pos(src_state.tok_to_pos[xc], c);
    
      tgt_state.pos_to_tok[a]  = xc;
      tgt_state.pos_to_tok[b]  = 0;
      tgt_state.pos_to_tok[c]  = xb;
      tgt_state.tok_to_pos[0]  = b;
      tgt_state.tok_to_pos[xb] = c;
      tgt_state.tok_to_pos[xc] = a;

      total_distance += P.dist[src_state.tok_to_pos[xb]][c];
      total_distance += P.dist[src_state.tok_to_pos[xc]][a];
      if(loop2_token(xb)) total_distance += 2;
      if(src_state.pos_to_tok[tgt_state.tok_to_pos[xb]] != xc &&
         loop2_token(xc)) total_distance += 2;
      hash ^= hash_pos(src_state.tok_to_pos[xb], c);
      hash ^= hash_pos(src_state.tok_to_pos[xc], a);

      last_direction_tgt ^= 1;

    }
  }
  
  CUDA_FN
  void undo_move(puzzle_data const& P, u8 move) {
    do_move(P, 6*(move/6) + (move+3)%6);
  }
};

