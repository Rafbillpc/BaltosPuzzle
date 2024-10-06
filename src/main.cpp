#include "header.hpp"
#include <omp.h>

struct puzzle_rot {
  u32 data[6];
};

struct puzzle_data {
  i32 n;
  u32 size;
  u32 center;

  vector<puzzle_rot> data;
  vector<vector<i32>> dist;

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
    
    data.resize(size);
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
    
    dist.resize(size);
    FOR(u, size) {
      vector<i32> Q(size); i32 nq = 0;
      vector<i32> D(size, 99999);
      auto add = [&](i32 v, i32 d) {
        if(d < D[v]) { D[v] = d; Q[nq++] = v; };
      };
      add(u,0);
      FOR(iq, size) {
        auto v = Q[iq];
        FOR(j, 6) add(data[v].data[j], D[v]+1);
      }

      // FOR(u, size) {
      //   FOR(j, 6) cerr << data[u].data[j] << ' ';
      //   cerr << endl;
      // }

      FOR(v, size) {
        runtime_assert(D[v] < 99999);
      }
      dist[u] = D;
    }

  }
};

const u32 MAX_SIZE = 2107;

struct puzzle_state {
  u32 pos_to_tok[MAX_SIZE];
  u32 tok_to_pos[MAX_SIZE];

  void reset(puzzle_data const& P) {
    FOR(i, P.size) {
      pos_to_tok[i] = (i == (i32)P.center ? 0 : (i < (i32)P.center ? 1+i : i));
    }

    update_tok_to_pos(P);
  }

  void update_tok_to_pos(puzzle_data const& P) {
    FOR(i, P.size) tok_to_pos[pos_to_tok[i]] = i;
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

map<i32, puzzle_state> load_configurations() {
  map<i32, puzzle_state> C; 
  
  ifstream is("StartingConfigurations.txt");
  runtime_assert(is.good());
  FORU(sz, 3, 27) {
    puzzle_data P;
    P.make(sz);
    { i32 sz_; is >> sz_; runtime_assert(sz == sz_); }
    puzzle_state S0;
    runtime_assert(P.size <= MAX_SIZE);
    FOR(i, P.size) {
      is >> S0.pos_to_tok[i];
    }
    S0.update_tok_to_pos(P);

    C[sz] = S0;
  }

  return C;
}

u64 hash_hole_src[MAX_SIZE];
u64 hash_hole_tgt[MAX_SIZE];
u64 hash_perm[MAX_SIZE][MAX_SIZE];

void init_hash() {
  FOR(i, MAX_SIZE) hash_hole_src[i] = rng.randomInt64();
  FOR(i, MAX_SIZE) hash_hole_tgt[i] = rng.randomInt64();
  FOR(i, MAX_SIZE) FOR(j, MAX_SIZE) hash_perm[i][j] = rng.randomInt64();
}

void init_rng() {
  u64 seed = time(0);
  
#pragma omp parallel
  {
    u64 offset = omp_get_thread_num();
    rng.reset(seed + offset);
  }
}

struct beam_state {
  puzzle_state src_state;
  puzzle_state tgt_state;
  u8 last_direction;

  i64 total_distance;
  u64 hash;

  void reset(puzzle_data const& P,
             puzzle_state const& initial_state,
             u8 initial_direction) {
    src_state = initial_state;
    tgt_state.reset(P);
    last_direction = initial_direction;

    total_distance = 0;
    FORU(u, 1, P.size-1) {
      total_distance += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    }

    hash = get_hash0(P);
  }

  i64 value(puzzle_data const& P) const {
    return total_distance;
    
    // i64 v = 0;
    // FORU(u, 1, P.size-1) {
    //   v += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    // }
    // runtime_assert(v == total_distance);
    // return v;
  }
  
  u64 get_hash0(puzzle_data const& P) const {
    u64 h = 0;
    h ^= hash_hole_src[src_state.tok_to_pos[0]];
    h ^= hash_hole_tgt[tgt_state.tok_to_pos[0]];
    FORU(i, 1, P.size-1) h ^= hash_perm[i][src_state.tok_to_pos[i]];
    return h;
  }

  u64 get_hash(puzzle_data const& P) const {
    return hash;
    // runtime_assert(get_hash0(P) == hash);
    // return get_hash0(P);
  }
  
  void do_move(puzzle_data const& P, u8 move) {
    i32 a = src_state.tok_to_pos[0], b, c;
    
    if(last_direction == 0) {
      b = P.data[a].data[move];
      c = P.data[a].data[(move+1)%6];
    }else{
      b = P.data[a].data[move];
      c = P.data[a].data[(move+5)%6];
    }

    auto xb = src_state.pos_to_tok[b];
    auto xc = src_state.pos_to_tok[c];

    total_distance -= P.dist[src_state.tok_to_pos[xb]][tgt_state.tok_to_pos[xb]];
    total_distance -= P.dist[src_state.tok_to_pos[xc]][tgt_state.tok_to_pos[xc]];
    hash ^= hash_hole_src[src_state.tok_to_pos[0]];
    hash ^= hash_perm[xb][src_state.tok_to_pos[xb]];
    hash ^= hash_perm[xc][src_state.tok_to_pos[xc]];
    
    src_state.pos_to_tok[a]  = xc;
    src_state.pos_to_tok[b]  = 0;
    src_state.pos_to_tok[c]  = xb;
    src_state.tok_to_pos[0]  = b;
    src_state.tok_to_pos[xb] = c;
    src_state.tok_to_pos[xc] = a;

    total_distance += P.dist[src_state.tok_to_pos[xb]][tgt_state.tok_to_pos[xb]];
    total_distance += P.dist[src_state.tok_to_pos[xc]][tgt_state.tok_to_pos[xc]];
    hash ^= hash_hole_src[src_state.tok_to_pos[0]];
    hash ^= hash_perm[xb][src_state.tok_to_pos[xb]];
    hash ^= hash_perm[xc][src_state.tok_to_pos[xc]];
    
    last_direction ^= 1;
  }
  
  void undo_move(puzzle_data const& P, u8 move) {
    do_move(P, (move+3)%6);
  }
};


const i32 TREE_SIZE  = 1'000'000;
const i32 LIMIT_SIZE = TREE_SIZE - 10'000;

using euler_tour_edge = u8;
struct euler_tour {
  i32              size;
  euler_tour_edge* data;

  FORCE_INLINE void reset() { size = 0; }
  FORCE_INLINE void push(i32 x) {
    data[size++] = x;
  }
  FORCE_INLINE u8& operator[](i32 ix) { return data[ix]; }
};
  
vector<euler_tour> tree_pool;
euler_tour get_new_tree(){
  euler_tour tour;
#pragma omp critical
  {
    if(tree_pool.empty()) {
      tour.size = 0;
      tour.data = new u8[TREE_SIZE];
    }else{
      tour = tree_pool.back();
      tour.size = 0;
      tree_pool.pop_back();
    }
  }
  return tour;
}

const u64 HASH_SIZE = 1ull<<28;
const u64 HASH_MASK = HASH_SIZE-1;
atomic<uint64_t> *HS = nullptr;

void traverse_euler_tour
(puzzle_data const& P,
 puzzle_state const& initial_state, u8 initial_direction,
 i32 istep,
 euler_tour tour_current,
 vector<euler_tour> &tours_next,
 i64* histogram,
 i32 cutoff, float cutoff_keep_probability)
{
  beam_state S; S.reset(P, initial_state, initial_direction);

  vector<i32> stack_moves(istep+1);
  i32 nstack_moves = 0;

  i32 ncommit = 0;
  if(tours_next.empty()) tours_next.eb(get_new_tree());
  auto *tour_next = &tours_next.back(); 

  FOR(iedge, tour_current.size) {
    auto const& edge = tour_current[iedge];
    if(edge > 0) {
      stack_moves[nstack_moves] = edge - 1;
      S.do_move(P, stack_moves[nstack_moves]);
      nstack_moves += 1;
    }else{
      if(nstack_moves == istep) {
        auto v = S.value(P);
        if(v < cutoff || (v == cutoff && rng.randomFloat() < cutoff_keep_probability)) {
          while(ncommit < nstack_moves) {
            tour_next->push(1+stack_moves[ncommit]);
            ncommit += 1;
          }

          // if(S.nvisited > best) {
          // #pragma omp critical
          //   {
          //     if(S.nvisited > best) {
          //       best = S.nvisited;
          //       sol.clear();
          //       FOR(i, nstack_moves) {
          //         i32 m = stack_moves[i];
          //         i32 dx = m/3, dy = m%3;
          //         sol += dc[dy][dx];
          //       }
          //     }
          //   }
          // }

          FOR(m, 6) {
            S.do_move(P, m);
            auto h = S.get_hash(P);
            auto prev = HS[h&HASH_MASK].exchange(h, std::memory_order_relaxed);
            if(prev != h) {
              histogram[S.value(P)] += 1;
              tour_next->push(1+m);
              tour_next->push(0);
            }
            S.undo_move(P, m);
          }
        }
      }

      if(nstack_moves == 0) {
        return;
      }

      if(ncommit == nstack_moves) {
        tour_next->push(0);
        ncommit -= 1;
      }
				
      nstack_moves -= 1;
      S.undo_move(P, stack_moves[nstack_moves]);

      if(tour_next->size > LIMIT_SIZE) {
        FORD(i,ncommit-1,0) tour_next->push(0);
        tour_next->push(0);
        tours_next.eb(get_new_tree());
        tour_next = &tours_next.back();
        ncommit = 0;
      }
    }
  }

  runtime_assert(false);
}

void beam_search
(puzzle_data const& P,
 puzzle_state const& initial_state,
 u8 initial_direction,
 i64 width)
{
  if(!HS) {
    auto ptr = new atomic<uint64_t>[HASH_SIZE];
    HS = ptr;
  }

  i32 max_score = 1000; // TODO
  vector<i64> histogram(max_score+1, 0);
  
  vector<euler_tour> tours_current;
  tours_current.eb(get_new_tree());
  tours_current.back().push(0);
	
  i32   cutoff = max_score;
  float cutoff_keep_probability = 1.0;

  for(i32 istep = 0;; ++istep) {
    debug(istep);

    timer timer_s;
    vector<euler_tour> tours_next;
    histogram.assign(max_score+1,0);

#pragma omp parallel
    {
      vector<euler_tour> L_tours_next;
      vector<i64> L_histogram(max_score+1,0);
      while(1) {
        euler_tour tour_current;
#pragma omp critical
        { if(!tours_current.empty()) {
            tour_current = tours_current.back();
            tours_current.pop_back();
          }else{
            tour_current.size = 0;
          }
        }
        if(tour_current.size == 0) break;

        traverse_euler_tour
          (P, initial_state, initial_direction,
           istep,
           tour_current, 
           L_tours_next, 
           L_histogram.data(),
           cutoff, cutoff_keep_probability);

        #pragma omp critical
        {
          tree_pool.eb(tour_current);
        }
      }
#pragma omp critical
      { if(!L_tours_next.empty()) {
          L_tours_next.back().push(0);
        }
        while(!L_tours_next.empty()) {
          tours_next.eb(L_tours_next.back());
          L_tours_next.pop_back();
        }
        FOR(i,max_score+1) histogram[i] += L_histogram[i];
      }
    }

    // if(best == 0) {
    //   debug("FOUND");
    //   return; // TODO: find solution
    // }
    
    { i64 total_count = 0;
      cutoff = max_score;
      cutoff_keep_probability = 1.0;
      FOR(i, max_score) {
        if(total_count+histogram[i] > width) {
          cutoff = i;
          cutoff_keep_probability = (float)(width-total_count) / (float)(histogram[i]);
          break;
        }
        total_count += histogram[i];
      }
    }

    i64 total_size = 0;
    for(auto const& t : tours_next) total_size += t.size;
    
    i32 low = max_score;
    FOR(i, max_score+1) if(histogram[i]) { low = i; break; }
    cerr << setw(3) << istep <<
      ": scores = " << setw(3) << low << ".." << setw(3) << cutoff <<
      ", tree size = " << setw(12) << total_size <<
      ", num trees = " << setw(6) << tours_next.size() <<
      ", elapsed = " << setw(10) << timer_s.elapsed() << "s" <<
      endl;

    if(low == 0) exit(0);
    
    tours_current = tours_next;
  }
}

void solve(puzzle_data const& P, puzzle_state initial_state) {
  const i64 WIDTH = 5'000'000;
  
  FOR(dir, 2) {
    debug(dir);
    beam_search
      (P, initial_state, dir, WIDTH);
  }
}

int main() {
  init_hash();
  init_rng();
  
  auto C = load_configurations();

  FORU(sz, 4, 4) {
    debug(sz);
    runtime_assert(C.count(sz));
    puzzle_data P;
    P.make(sz);
    solve(P, C[sz]);
  }

  return 0;
}
