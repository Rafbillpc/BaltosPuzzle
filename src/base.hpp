#pragma once
#include "header.hpp"

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

  void make(i32 n_) {
    n = n_;
    map<array<i32, 2>, u32> M;
    size = 0;
    
    FOR(u, 2*n-1) {
      u32 ncol = 2*n-1 - abs(u-(n-1));
      FOR(icol, ncol) {
        i32 v = icol + max(0, u - (n-1));

        M[{u,v}] = size;

        M[{u + 2*n-1, v + n-1}] = size;
        M[{u + n-1, v - n}] = size;
        M[{u - n, v - (2*n-1)}] = size;
        M[{u - (2*n-1), v - (n-1)}] = size;
        M[{u - (n-1), v + n}] = size;
        M[{u + n, v + (2*n-1)}] = size;

        size += 1;
      }
    }

    center = M[{n-1,n-1}];
    
    FOR(u, 2*n-1) {
      u32 ncol = 2*n-1 - abs(u-(n-1));
      FOR(icol, ncol) {
        i32 v = icol + max(0, u - (n-1));

        u32 ix = M[{u,v}];
        u32 at[6] =
          { M[{u-1,v}],
            M[{u,v+1}],
            M[{u+1,v+1}],
            M[{u+1,v}],
            M[{u,v-1}],
            M[{u-1,v-1}],
          };
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
              dist[u][w] = min(dist[u][w], dx*dx+dy*dy);
              w = data[w].data[y];
            }
          }
          v = data[v].data[x];
        }
      }
    }
  }
};

namespace automaton {
  const u32 NUM_STATES = 7*7;

  extern
  u32 next_state[NUM_STATES][12];

  extern
  u32 allow_move[NUM_STATES];

  static inline
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

  CUDA_FN
  void reset(puzzle_data const& P) {
    FOR(i, P.size) {
      pos_to_tok[i] = (i == (i32)P.center ? 0 : (i < (i32)P.center ? 1+i : i));
    }

    update_tok_to_pos(P.size);
  }

  CUDA_FN
  void update_tok_to_pos(i32 size) {
    FOR(i, size) tok_to_pos[pos_to_tok[i]] = i;
  }

  void print(puzzle_data const& P) const {

    i32 ix = 0;
    FOR(u, 2*P.n-1) {
      u32 ncol = 2*P.n-1 - abs(u-(P.n-1));
      FOR(icol, ncol) {
        cerr << pos_to_tok[ix] << ' ';
        ix += 1;
      }
      cerr << endl;
    }

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
