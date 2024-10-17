#include "header.hpp"
#include "puzzle.hpp"

const u8 INF = 255;

puzzle_data puzzle;

vector<i32> get_solve_order() {
  vector<i32> order;
  
  FOR(layer, puzzle.n-1) {
    i32 u = layer, v = layer;
    FOR(d, 6) {
      FOR(k, puzzle.n-1-layer) {
        order.eb(puzzle.from_coord.at({u,v}));
        u += du[d], v += dv[d];
      }
    }
  }
  runtime_assert(order.size() == puzzle.size-1);

  return order;
}

struct distances_t {
  u8 dist[MAX_SIZE][MAX_SIZE][2];
  u8 from[MAX_SIZE][MAX_SIZE][2];

  u8& operator()(i32 u, i32 v, i32 c) { return dist[u][v][c]; }
  u8 const& operator()(i32 u, i32 v, i32 c) const { return dist[u][v][c]; }
};

void compute_distances_to
(vector<i32> const& order,
 vector<i32> const& active_cells,
 i32 u,
 distances_t& distance)
{
  FOR(i, puzzle.size) FOR(j, puzzle.size) FOR(c,2) {
    distance(i,j,c) = INF;
    distance.from[i][j][c] = 0;
  }

  vector<array<i32, 3>> Q;
  Q.reserve(puzzle.size * puzzle.size * 2);

  auto add = [&](i32 a, i32 b, i32 c, u8 di, u8 from) {
    runtime_assert(di < INF);
    if(di < distance(a,b,c)) {
      distance(a,b,c) = di;
      Q.push_back({a,b,c});
    }
    if(di == distance(a,b,c)) {
      distance.from[a][b][c] |= from;
    }
  };

  FOR(b, puzzle.size) if(active_cells[b] && b != u) {
    FOR(c, 2) add(u, b, c, 0, 0);
  }
  
  FOR(iq, Q.size()) {
    auto [v,a,d] = Q[iq];
    
    FOR(m, 6) {
      i32 b, c;
    
      if(d == 0) {
        b = puzzle.data[a].data[m];
        c = puzzle.data[a].data[(m+1)%6];
      }else{
        b = puzzle.data[a].data[m];
        c = puzzle.data[a].data[(m+5)%6];
      }

      if(!active_cells[b] || !active_cells[c]) continue;

      i32 w = v;
      if(v == b) w = c;
      if(v == c) w = a;

      add(w, b, d^1, distance(v,a,d)+1, bit((m+3)%6));
    }
  }
}
  
vector<distances_t> compute_distances(vector<i32> const& order) {
  vector<i32> active_cells(puzzle.size, 1);
  vector<distances_t> distances(order.size());
  FOR(i, order.size()) {
    debug(i);
    i32 u = order[i];
    // TODO: cache mmap
    compute_distances_to(order, active_cells, u, distances[i]);
    active_cells[u] = 0;
  }
  return distances;
}

struct state {
  puzzle_state src, tgt;
  u8 src_dir;
  u8 tgt_dir;

  u32 total_distance;
  u32 cost = 0;

  void init() {
    total_distance = 0;
    FORU(i, 1, puzzle.size-1) {
      total_distance += puzzle.dist[src.tok_to_pos[i]][tgt.tok_to_pos[i]];
    }
  }

  u32 value() const {
    return 5 * cost + 2 * total_distance;
  }

  void do_move_src(u8 move) {
    u32 a = src.tok_to_pos[0], b, c;
    
    if(src_dir == 0) {
      b = puzzle.data[a].data[move];
      c = puzzle.data[a].data[(move+1)%6];
    }else{
      b = puzzle.data[a].data[move];
      c = puzzle.data[a].data[(move+5)%6];
    }

    u32 xb = src.pos_to_tok[b];
    u32 xc = src.pos_to_tok[c];

    total_distance -= puzzle.dist[b][tgt.tok_to_pos[xb]];
    total_distance -= puzzle.dist[c][tgt.tok_to_pos[xc]];
      
    src.pos_to_tok[a]  = xc;
    src.pos_to_tok[b]  = 0;
    src.pos_to_tok[c]  = xb;
    src.tok_to_pos[0]  = b;
    src.tok_to_pos[xb] = c;
    src.tok_to_pos[xc] = a;

    total_distance += puzzle.dist[c][tgt.tok_to_pos[xb]];
    total_distance += puzzle.dist[a][tgt.tok_to_pos[xc]];

    src_dir ^= 1;
  }
 
  void do_move_tgt(u8 move) {
    u32 a = tgt.tok_to_pos[0], b, c;
      
    if(tgt_dir == 0) {
      b = puzzle.data[a].data[move];
      c = puzzle.data[a].data[(move+1)%6];
    }else{
      b = puzzle.data[a].data[move];
      c = puzzle.data[a].data[(move+5)%6];
    }

    u32 xb = tgt.pos_to_tok[b];
    u32 xc = tgt.pos_to_tok[c];

    total_distance -= puzzle.dist[src.tok_to_pos[xb]][b];
    total_distance -= puzzle.dist[src.tok_to_pos[xc]][c];
   
    tgt.pos_to_tok[a]  = xc;
    tgt.pos_to_tok[b]  = 0;
    tgt.pos_to_tok[c]  = xb;
    tgt.tok_to_pos[0]  = b;
    tgt.tok_to_pos[xb] = c;
    tgt.tok_to_pos[xc] = a;

    total_distance += puzzle.dist[src.tok_to_pos[xb]][c];
    total_distance += puzzle.dist[src.tok_to_pos[xc]][a];
      
    tgt_dir ^= 1;
  }
  
  template<class F>
  void children
  (distances_t const& dist,
   vector<i32> const& active_cells,
   u32 u,
   F&& f
   )
  {
    auto v = value();

    vector<tuple<i32, i32> > as;
    
    FORU(a, 1, puzzle.size - 1) {
      if(!active_cells[src.tok_to_pos[a]]) continue;
      if(!active_cells[tgt.tok_to_pos[a]]) continue;

      if(dist(src.tok_to_pos[a], src.tok_to_pos[0], src_dir) == INF ||
         dist(tgt.tok_to_pos[a], tgt.tok_to_pos[0], tgt_dir) == INF) continue;

      as.eb(dist(src.tok_to_pos[a], src.tok_to_pos[0], src_dir) +
            dist(tgt.tok_to_pos[a], tgt.tok_to_pos[0], tgt_dir), a);
    }

    for(auto [d,a] : as) {
      static u8 moves_src[256]; i32 nmoves_src = 0;
      static u8 moves_tgt[256]; i32 nmoves_tgt = 0;

      { while(dist(src.tok_to_pos[a], src.tok_to_pos[0], src_dir) != 0){
          u8 f = dist.from[src.tok_to_pos[a]][src.tok_to_pos[0]][src_dir];
          runtime_assert(f != 0);
          u8 m;
          do { m = rng.random32(6); } while(!(f & bit(m)));
          moves_src[nmoves_src++] = m;
          do_move_src(m);
          cost += 1;
        }
      }

      { while(dist(tgt.tok_to_pos[a], tgt.tok_to_pos[0], tgt_dir) != 0){
          u8 f = dist.from[tgt.tok_to_pos[a]][tgt.tok_to_pos[0]][tgt_dir];
          runtime_assert(f != 0);
          u8 m;
          do { m = rng.random32(6); } while(!(f & bit(m)));
          moves_tgt[nmoves_tgt++] = m;
          do_move_tgt(m);
          cost += 1;
        }
      }

      f(*this);

      while(nmoves_src) {
        u8 m = moves_src[--nmoves_src];
        do_move_src((m+3)%6);
        cost -= 1;
      }

      while(nmoves_tgt) {
        u8 m = moves_tgt[--nmoves_tgt];
        do_move_tgt((m+3)%6);
        cost -= 1;
      }
    }

    init();
    runtime_assert(value() == v);
  }
};

void solve(puzzle_state const& initial_state)
{
  // Compute the solve order
  auto order = get_solve_order();
  
  debug(puzzle.size * puzzle.size * puzzle.size * 2);

  // At every stage, compute the min distance to solve a cell
  auto distances = compute_distances(order);

  // Solve

  const i32 WIDTH = 64;
  
  vector<state> BEAM;
  { state I;
    I.src = initial_state;
    I.tgt.reset(puzzle);
    I.src_dir = 0;
    I.tgt_dir = 0;
    I.init();
    BEAM.eb(I);
  }

  vector<i32> active_cells(puzzle.size, 1);

  FOR(i, order.size()) {
    auto const& dist = distances[i];
    auto u = order[i];

    vector<state> NEW_BEAM;
    vector<i32> BEAM_I;
    FOR(ia, BEAM.size()) {
      auto S = BEAM[ia];
      S.children(dist, active_cells, u, [&](state const& T) {
        BEAM_I.pb(NEW_BEAM.size());
        NEW_BEAM.pb(T);
      });
    }

    rng.shuffle(BEAM_I);
    sort(all(BEAM_I), [&](i32 i, i32 j){
      return NEW_BEAM[i].value() < NEW_BEAM[j].value();
    });
    if(BEAM_I.size() > WIDTH) {
      BEAM_I.resize(WIDTH);
    }

    BEAM.resize(BEAM_I.size());
    FOR(i, BEAM_I.size()) BEAM[i] = NEW_BEAM[BEAM_I[i]];

    if(BEAM.size() > 0) {
      debug(i, u, BEAM[0].cost, BEAM[0].total_distance);
    }
    
    active_cells[u] = 0;
  }
  
  // puzzle_state src, tgt;
  // src = initial_state;
  // tgt.reset(puzzle);

  // u8 src_dir = 0, tgt_dir = 0;
  
  // vector<i32> active_cells(puzzle.size, 1);
  // i32 total = 0;

  // FOR(i, order.size()) {
  //   auto const& dist = distances[i];
  //   auto u = order[i];
  //   debug(i, u, total);

  //   auto h1 = src.tok_to_pos[0];
  //   auto h2 = tgt.tok_to_pos[0];

  //   i32 bd = 100;
  //   i32 ba = -1;
  //   i32 count = 0;
    
  //   FORU(a, 1, puzzle.size - 1) {
  //     auto cell1 = src.tok_to_pos[a];
  //     auto cell2 = tgt.tok_to_pos[a];
  //     if(!active_cells[cell1]) continue;
  //     runtime_assert(active_cells[cell2]);

  //     auto d =
  //       (i32)dist(cell1, h1, src_dir) +
  //       (i32)dist(cell2, h2, tgt_dir);
  //     if(d < bd) {
  //       bd = d;
  //       count = 0;
  //     }
  //     if(d == bd) {
  //       count += 1;
  //       if(rng.random32(count) == 0) {
  //         ba = a;
  //         debug(d, a, cell1, h1, cell2, h2);
  //       }
  //     }
  //   }

  //   total += bd;
  //   debug(bd, ba);
  //   if(ba == -1) break;

  //   { while(dist(src.tok_to_pos[ba], src.tok_to_pos[0], src_dir) != 0){
  //       debug((int)dist(src.tok_to_pos[ba], src.tok_to_pos[0], src_dir));
  //       u8 f = dist.from[src.tok_to_pos[ba]][src.tok_to_pos[0]][src_dir];
  //       u8 m;
  //       do {
  //         m = rng.random32(6);
  //       }while(!(f&bit(m)));
  //       debug((int)m);
  //       src.do_move(puzzle, m, src_dir);
  //       src_dir ^= 1;
  //     }
  //   }

  //   { while(dist(tgt.tok_to_pos[ba], tgt.tok_to_pos[0], tgt_dir) != 0){
  //       debug((int)dist(tgt.tok_to_pos[ba], tgt.tok_to_pos[0], tgt_dir));
  //       u8 f = dist.from[tgt.tok_to_pos[ba]][tgt.tok_to_pos[0]][tgt_dir];
  //       u8 m;
  //       do {
  //         m = rng.random32(6);
  //       }while(!(f&bit(m)));
  //       debug((int)m);
  //       tgt.do_move(puzzle, m, tgt_dir);
  //       tgt_dir ^= 1;
  //     }
  //   }

  //   runtime_assert(src.pos_to_tok[u] == tgt.pos_to_tok[u]);
  //   active_cells[u] = 0;

  //   src.print(puzzle);
  //   tgt.print(puzzle);
  // }
}

int main(int argc, char** argv) {
  runtime_assert(argc == 2);

  i64 sz = atoi(argv[1]);
  
  automaton::init();
  
  auto C = load_configurations();

  runtime_assert(C.count(sz));
  puzzle.make(sz);

  solve(C[sz]);
  
  return 0;
}
