#include "puzzle.hpp"
#include "eval.hpp"

puzzle_data puzzle;

void puzzle_data::make(i32 n_) {
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

  FOR(ix, size) {
    auto [u,v] = to_coord[ix];
    FOR(d, 6) rot[ix][d] = from_coord[{u+du[d],v+dv[d]}];
  }

  u32 dist_feature_key[27][27];
  i32 next_feature = 0;
  FOR(x, 27) FOR(y, x+1) if(x+y < 27) {
    dist_feature_key[x][y] = next_feature++;
  }
  runtime_assert(next_feature == NUM_FEATURES);
  
  FOR(u, size) FOR(v, size) dist[u][v] = 999'999'999;
  FOR(u, size) {
    FOR(x, 6) {
      i32 v = u;
      FORU(dx, 0, 2*n) {
        FOR(y, 6) {
          i32 w = v;
          FORU(dy, 0, 2*n) {

            i32 di = dx+dy;

            i32 delta_u = dx * du[x] + dy * du[y];
            i32 delta_v = dx * dv[x] + dy * dv[y];

            if(di < (i32)dist[u][w]) {
              dist[u][w] = di;

              i32 a = max(dx, dy);
              i32 b = min(dx, dy);
              
              dist_feature[u][w] = dist_feature_key[a][b];
            }
              
            w = rot[w][y];
          }
        }
        v = rot[v][x];
      }
    }
  }

  // FOR(i, size) FOR(j, size) {
  //   auto [u,v] = dist_delta[i][j];
  //   array<i32, 2> key = {u, v};
  //   runtime_assert(from_coord.count(key));
  //   dist_key[i][j] = from_coord[key];
  // }
  
  FOR(i, size) {
    tgt_pos_to_tok[i] = (i == (i32)center ? 0 : (i < (i32)center ? 1+i : i));
  }
  FOR(i, size) {
    tgt_tok_to_pos[tgt_pos_to_tok[i]] = i;
  }

  num_features = (n+1) * n / 2;
}

void puzzle_state::set_tgt() {
  FOR(i, puzzle.size) {
    tok_to_pos[i] = puzzle.tgt_tok_to_pos[i];
    pos_to_tok[i] = puzzle.tgt_pos_to_tok[i];
  }
}

void puzzle_state::print() const {
  i32 sz = 1+log10(puzzle.size);
  string spaces = "";
  FOR(i, sz) spaces += ' ';
  
  i32 ix = 0;
  FOR(u, 2*puzzle.n-1) {
    u32 ncol = 2*puzzle.n-1 - abs(u-(puzzle.n-1));
    FOR(i, abs(u-(puzzle.n-1))) cerr << spaces;
    FOR(icol, ncol) {
      if(pos_to_tok[ix] == (u32)puzzle.tgt_pos_to_tok[ix]) {
        cout << "\033[1;32m" << setw(sz) << pos_to_tok[ix] << "\033[0m" << spaces;
      }else{
        cerr << setw(sz) << pos_to_tok[ix] << spaces;
      }
      ix += 1;
    }
    cerr << endl;
  }
}

bool puzzle_state::get_parity() const {
  bool parity = 0;
  vector<u32> vis(puzzle.size, 0);
  FOR(i, puzzle.size) if(!vis[i]) {
    u32 sz = 0;
    for(u32 j = i; !vis[j]; j = pos_to_tok[j]) {
      vis[j] = 1;
      sz += 1;
    }
    parity ^= ((sz+1) & 1);
  }
  return parity;
}

void puzzle_state::generate(u64 seed) {
  RNG rng; rng.reset(seed);

  set_tgt();
  u32 goal_parity = get_parity();
  
  while(1) {
    FOR(i, puzzle.size) pos_to_tok[i] = i;
    rng.shuffle(pos_to_tok, pos_to_tok + puzzle.size);
    FOR(i, puzzle.size) tok_to_pos[pos_to_tok[i]] = i;

    u32 parity = get_parity();
    if(parity == goal_parity) break;
  }
}

map<i32, puzzle_state> load_configurations(){
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
    FOR(i, size) {
      S0.tok_to_pos[S0.pos_to_tok[i]] = i;
    }

    C[n] = S0;
  }

  return C;
}
