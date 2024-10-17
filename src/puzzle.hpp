#pragma once
#include "header.hpp"

const i32 du[6] = {-1,0,1,1,0,-1};
const i32 dv[6] = {0,1,1,0,-1,-1};

const u32 MAX_SIZE = 2107;
const u32 MAX_SOLUTION_SIZE = 50'000;

struct puzzle_rot {
  u32 data[6];
};

struct puzzle_data {
  i32 n;
  u32 size;
  u32 center;

  puzzle_rot data[MAX_SIZE];
  i32 dist[MAX_SIZE][MAX_SIZE];
  array<i32,2> dist_delta[MAX_SIZE][MAX_SIZE];

  array<i32, 2> to_coord[MAX_SIZE];
  map<array<i32, 2>, u32> from_coord;

  i32 tgt_tok_to_pos[MAX_SIZE];
  i32 tgt_pos_to_tok[MAX_SIZE];
  i32 rot_weight[MAX_SIZE];
  
  void make(i32 n_) {
    n = n_;
    size = 0;
    from_coord.clear();
    
    FOR(u, 2*n-1) {
      u32 ncol = 2*n-1 - abs(u-(n-1));
      FOR(icol, ncol) {
        i32 v = icol + max(0, u - (n-1));

        to_coord[size] = {u,v};
        from_coord[{u,v}] = size;

        from_coord[{u + 2*n-1, v + n-1}] = size;
        from_coord[{u + n-1, v - n}] = size;
        from_coord[{u - n, v - (2*n-1)}] = size;
        from_coord[{u - (2*n-1), v - (n-1)}] = size;
        from_coord[{u - (n-1), v + n}] = size;
        from_coord[{u + n, v + (2*n-1)}] = size;

        size += 1;
      }
    }

    center = from_coord[{n-1,n-1}];
    
    FOR(u, 2*n-1) {
      u32 ncol = 2*n-1 - abs(u-(n-1));
      FOR(icol, ncol) {
        i32 v = icol + max(0, u - (n-1));

        u32 ix = from_coord[{u,v}];
        u32 at[6];
        FOR(d,6) at[d] = from_coord[{u+du[d],v+dv[d]}];
          // { from_coord[{u-1,v}],
          //   from_coord[{u,v+1}],
          //   from_coord[{u+1,v+1}],
          //   from_coord[{u+1,v}],
          //   from_coord[{u,v-1}],
          //   from_coord[{u-1,v-1}],
          // };
        FOR(j, 6) data[ix].data[j] = at[j];
      }
    }

    FOR(u, size) FOR(v, size) dist[u][v] = 999'999'999;
    FOR(u, size) {
      FOR(x, 6) {
        i32 v = u;
        FORU(dx, 0, 2*n) {
          FOR(y, 6) {
            i32 w = v;
            FORU(dy, 0, 2*n) {

              i32 delta_u = 0, delta_v = 0;
              delta_u += dx * du[x] + dy * du[y];
              delta_v += dx * dv[x] + dy * dv[y];

              i32 di = dx+dy;

              if(di < dist[u][w]) {
                dist[u][w] = di;
                dist_delta[u][w] = {delta_u,delta_v};
              }
              
              w = data[w].data[y];
            }
          }
          v = data[v].data[x];
        }
      }
    }
    
    FOR(i, size) {
      tgt_pos_to_tok[i] = (i == (i32)center ? 0 : (i < (i32)center ? 1+i : i));
    }
    FOR(i, size) {
      tgt_tok_to_pos[tgt_pos_to_tok[i]] = i;
    }

    
    rot_weight[0] = 0;
    FOR(layer, n-1) {
      i32 u = layer, v = layer;
      FOR(d, 6) {
        FOR(k, n-1-layer) {
          rot_weight[tgt_pos_to_tok[from_coord.at({u,v})]] = (n-layer);
          u += du[d], v += dv[d];
        }
      }
    }
  }
};

namespace automaton {
  const u32 NUM_STATES = 7*7;
  
  u32 next_state[NUM_STATES][12];
  u32 allow_move[NUM_STATES];

  void init() {
    FOR(i, NUM_STATES) allow_move[i] = bit(12)-1;
    FOR(x, 7) FOR(y, 7) {
      u32 xy = x*6+y;
      if(x != 6) allow_move[xy] ^= bit((x+3)%6);
      if(y != 6) allow_move[xy] ^= bit(6 + (y+3)%6);
      FOR(m, 6) next_state[xy][m] = m*6+y;
      FOR(m, 6) next_state[xy][6+m] = x*6+m;
    }
  }

};

struct puzzle_state {
  u32 pos_to_tok[MAX_SIZE];
  u32 tok_to_pos[MAX_SIZE];

  void reset(puzzle_data const& P) {
    FOR(i, P.size) {
      pos_to_tok[i] = (i == (i32)P.center ? 0 : (i < (i32)P.center ? 1+i : i));
    }

    update_tok_to_pos(P.size);
  }

  void update_tok_to_pos(i32 size) {
    FOR(i, size) tok_to_pos[pos_to_tok[i]] = i;
  }

  void print(puzzle_data const& P) const {

    i32 ix = 0;
    FOR(u, 2*P.n-1) {
      u32 ncol = 2*P.n-1 - abs(u-(P.n-1));
      FOR(i, abs(u-(P.n-1))) cerr << "   ";
      FOR(icol, ncol) {
        cerr << setw(3) << pos_to_tok[ix] << "   ";
        ix += 1;
      }
      cerr << endl;
    }
  }

   
  CUDA_FN
  void do_move(puzzle_data const& P, u8 move, u8 dir) {
    u32 a = tok_to_pos[0], b, c;
    
    if(dir == 0) {
      b = P.data[a].data[move];
      c = P.data[a].data[(move+1)%6];
    }else{
      b = P.data[a].data[move];
      c = P.data[a].data[(move+5)%6];
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

};

static inline
map<i32, puzzle_state> load_configurations() {
  map<i32, puzzle_state> C; 
  
  ifstream is("StartingConfigurations.txt");
  runtime_assert(is.good());
  FORU(n, 3, 27) {
    { i32 n_; is >> n_; runtime_assert(n == n_); }

    i32 size = 0;
    FOR(u, 2*n-1) {
      u32 ncol = 2*n-1 - abs(u-(n-1));
      size += ncol;
    }

    puzzle_state S0;
    FOR(i, size) {
      is >> S0.pos_to_tok[i];
    }
    S0.update_tok_to_pos(size);

    C[n] = S0;
  }

  return C;
}

