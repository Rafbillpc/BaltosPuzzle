#include "header.hpp"
#include <memory>
#include <omp.h>

const u32 MAX_SIZE = 2107;

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

u64 hash_pos[MAX_SIZE][MAX_SIZE];

void init_hash() {
  FOR(i, MAX_SIZE) FOR(j, MAX_SIZE) hash_pos[i][j] = rng.randomInt64();
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
  u8 last_direction_src;
  u8 last_direction_tgt;

  i64 total_distance;
  u64 hash;

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

    hash = get_hash0(P);
  }

  i64 value(puzzle_data const& P) const {
    return total_distance + (last_direction_src != last_direction_tgt);
    
    // i64 v = 0;
    // FORU(u, 1, P.size-1) {
    //   v += P.dist[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    // }
    // runtime_assert(v == total_distance);
    // return v;
  }
  
  u64 get_hash0(puzzle_data const& P) const {
    u64 h = 0;
    FORU(u, 1, P.size-1) h ^= hash_pos[src_state.tok_to_pos[u]][tgt_state.tok_to_pos[u]];
    return h;
  }

  u64 get_hash(puzzle_data const& P) const {
    return hash;
  }
  
  tuple<i64, u64> plan_move(puzzle_data const& P, u8 move) const {
    i64 v = total_distance; u64 h = hash;
    
    if(move < 6) {
      i32 a = src_state.tok_to_pos[0], b, c;
    
      if(last_direction_src == 0) {
        b = P.data[a].data[move];
        c = P.data[a].data[(move+1)%6];
      }else{
        b = P.data[a].data[move];
        c = P.data[a].data[(move+5)%6];
      }

      auto xb = src_state.pos_to_tok[b];
      auto xc = src_state.pos_to_tok[c];

      v -= P.dist[b][tgt_state.tok_to_pos[xb]];
      v -= P.dist[c][tgt_state.tok_to_pos[xc]];
      h ^= hash_pos[b][tgt_state.tok_to_pos[xb]];
      h ^= hash_pos[c][tgt_state.tok_to_pos[xc]];

      v += P.dist[c][tgt_state.tok_to_pos[xb]];
      v += P.dist[a][tgt_state.tok_to_pos[xc]];
      h ^= hash_pos[c][tgt_state.tok_to_pos[xb]];
      h ^= hash_pos[a][tgt_state.tok_to_pos[xc]];
    }else{
      i32 a = tgt_state.tok_to_pos[0], b, c;
      move -= 6;
      
      if(last_direction_tgt == 0) {
        b = P.data[a].data[move];
        c = P.data[a].data[(move+1)%6];
      }else{
        b = P.data[a].data[move];
        c = P.data[a].data[(move+5)%6];
      }

      auto xb = tgt_state.pos_to_tok[b];
      auto xc = tgt_state.pos_to_tok[c];

      v -= P.dist[src_state.tok_to_pos[xb]][b];
      v -= P.dist[src_state.tok_to_pos[xc]][c];
      h ^= hash_pos[src_state.tok_to_pos[xb]][b];
      h ^= hash_pos[src_state.tok_to_pos[xc]][c];

      v += P.dist[src_state.tok_to_pos[xb]][c];
      v += P.dist[src_state.tok_to_pos[xc]][a];
      h ^= hash_pos[src_state.tok_to_pos[xb]][c];
      h ^= hash_pos[src_state.tok_to_pos[xc]][a];
    }

    return mt(v + (last_direction_src == last_direction_tgt), h);
  }
    
  void do_move(puzzle_data const& P, u8 move) {
    if(move < 6) {
      i32 a = src_state.tok_to_pos[0], b, c;
    
      if(last_direction_src == 0) {
        b = P.data[a].data[move];
        c = P.data[a].data[(move+1)%6];
      }else{
        b = P.data[a].data[move];
        c = P.data[a].data[(move+5)%6];
      }

      auto xb = src_state.pos_to_tok[b];
      auto xc = src_state.pos_to_tok[c];

      total_distance -= P.dist[b][tgt_state.tok_to_pos[xb]];
      total_distance -= P.dist[c][tgt_state.tok_to_pos[xc]];
      hash ^= hash_pos[b][tgt_state.tok_to_pos[xb]];
      hash ^= hash_pos[c][tgt_state.tok_to_pos[xc]];

      src_state.pos_to_tok[a]  = xc;
      src_state.pos_to_tok[b]  = 0;
      src_state.pos_to_tok[c]  = xb;
      src_state.tok_to_pos[0]  = b;
      src_state.tok_to_pos[xb] = c;
      src_state.tok_to_pos[xc] = a;

      total_distance += P.dist[c][tgt_state.tok_to_pos[xb]];
      total_distance += P.dist[a][tgt_state.tok_to_pos[xc]];
      hash ^= hash_pos[c][tgt_state.tok_to_pos[xb]];
      hash ^= hash_pos[a][tgt_state.tok_to_pos[xc]];

      last_direction_src ^= 1;

    }else{
      i32 a = tgt_state.tok_to_pos[0], b, c;
      move -= 6;
      
      if(last_direction_tgt == 0) {
        b = P.data[a].data[move];
        c = P.data[a].data[(move+1)%6];
      }else{
        b = P.data[a].data[move];
        c = P.data[a].data[(move+5)%6];
      }

      auto xb = tgt_state.pos_to_tok[b];
      auto xc = tgt_state.pos_to_tok[c];

      total_distance -= P.dist[src_state.tok_to_pos[xb]][b];
      total_distance -= P.dist[src_state.tok_to_pos[xc]][c];
      hash ^= hash_pos[src_state.tok_to_pos[xb]][b];
      hash ^= hash_pos[src_state.tok_to_pos[xc]][c];
    
      tgt_state.pos_to_tok[a]  = xc;
      tgt_state.pos_to_tok[b]  = 0;
      tgt_state.pos_to_tok[c]  = xb;
      tgt_state.tok_to_pos[0]  = b;
      tgt_state.tok_to_pos[xb] = c;
      tgt_state.tok_to_pos[xc] = a;

      total_distance += P.dist[src_state.tok_to_pos[xb]][c];
      total_distance += P.dist[src_state.tok_to_pos[xc]][a];
      hash ^= hash_pos[src_state.tok_to_pos[xb]][c];
      hash ^= hash_pos[src_state.tok_to_pos[xc]][a];

      last_direction_tgt ^= 1;

    }
  }
  
  void undo_move(puzzle_data const& P, u8 move) {
    do_move(P, 6*(move/6) + (move+3)%6);
  }
};

const i64 MIN_TREE_SIZE  = 1<<17;
i64 tree_size = MIN_TREE_SIZE;

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
};
  
vector<euler_tour> tree_pool;
euler_tour get_new_tree(){
  euler_tour tour;
#pragma omp critical
  {
    if(tree_pool.empty()) {
      tour.max_size = tree_size;
      tour.size = 0;
      tour.data = new u8[tree_size];
    }else{
      tour = tree_pool.back();
      tour.size = 0;
      tree_pool.pop_back();
    }
  }
  return tour;
}

const u64 HASH_SIZE = 1ull<<30;
const u64 HASH_MASK = HASH_SIZE-1;
uint64_t *HS = nullptr;


vector<u8> find_solution
(puzzle_data const& P,
 puzzle_state const& initial_state, u8 initial_direction,
 i32 istep,
 euler_tour tour_current)
{
  beam_state S; S.reset(P, initial_state, initial_direction);

  vector<u8> stack_moves(istep+1, 255);
  i32 nstack_moves = 0;

  FOR(iedge, tour_current.size) {
    auto const& edge = tour_current[iedge];
    if(edge > 0) {
      stack_moves[nstack_moves] = edge - 1;
      S.do_move(P, stack_moves[nstack_moves]);
      nstack_moves += 1;
    }else{
      if(nstack_moves == istep+1) {
        auto v = S.value(P);
        if(v == 0) {
          return stack_moves;
        }
      }

      if(nstack_moves == 0) {
        return {};
      }
				
      nstack_moves -= 1;
      S.undo_move(P, stack_moves[nstack_moves]);
    }
  }

  runtime_assert(false);
}

void traverse_euler_tour
(puzzle_data const& P,
 puzzle_state const& initial_state, u8 initial_direction,
 i32 istep,
 euler_tour tour_current,
 vector<euler_tour> &tours_next,
 i64* histogram, i64& low, i64& high,
 i32 cutoff, float cutoff_keep_probability)
{
  beam_state S; S.reset(P, initial_state, initial_direction);

  vector<u8> stack_moves(istep+1);
  i32 nstack_moves = 0;

  i32 ncommit = 0;
  if(tours_next.empty()) tours_next.eb(get_new_tree());
  auto *tour_next = &tours_next.back(); 
  runtime_assert(tour_next->max_size == tree_size);

  FOR(iedge, tour_current.size) {
    auto const& edge = tour_current[iedge];
    if(edge > 0) {
      // if(__builtin_expect(stack_moves[nstack_moves] == edge - 1, false) &&
      //    tour_next->size > 0 &&
      //    tour_next->data[tour_next->size-1] == 0)
      //   {
      //     tour_next->size -= 1;
      //   }else{
      stack_moves[nstack_moves] = edge - 1;
      // }
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

          FOR(m, 12) {
            auto [v,h] = S.plan_move(P, m);
            auto prev = HS[h&HASH_MASK];
            if(prev != h) {
              HS[h&HASH_MASK] = h;
              low = min(low, v);
              high = max(high, v);
              histogram[v] += 1;
              tour_next->push(1+m);
              tour_next->push(0);
            }
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
    }
    
    if(__builtin_expect(tour_next->size + 2 * istep + 128 > tree_size, false)) {
      FORD(i,ncommit-1,0) tour_next->push(0);
      tour_next->push(0);
      tours_next.eb(get_new_tree());
      tour_next = &tours_next.back();
      ncommit = 0;
    }

  }

  runtime_assert(false);
}

vector<u8> beam_search
(puzzle_data const& P,
 puzzle_state const& initial_state,
 u8 initial_direction,
 i64 width)
{
  if(!HS) {
    auto ptr = new uint64_t[HASH_SIZE];
    HS = ptr;
  }

  beam_state S; S.reset(P, initial_state, initial_direction);
  i32 max_score = S.total_distance + 1000; // TODO
  debug(max_score);
  vector<i64> histogram(max_score+1, 0);
  
  vector<euler_tour> tours_current;
  tours_current.eb(get_new_tree());
  tours_current.back().push(0);
	
  i32   cutoff = max_score;
  float cutoff_keep_probability = 1.0;

  i32 num_threads = omp_get_max_threads();
  debug(num_threads);

  vector<vector<i64>> L_histograms(num_threads, vector<i64>(max_score+1, 0));

  for(i32 istep = 0;; ++istep) {
    timer timer_s;
    vector<euler_tour> tours_next;

    bool increased_tree_size = false;
    if(tours_current.size() > 128) {
      for(auto &t : tree_pool) delete[] t.data;
      tree_pool.clear();
      tree_size *= 2;
      increased_tree_size = true;
    }

    sort(all(tours_current), [&](auto const& t1, auto const& t2) {
      return t1.size < t2.size;
    });

    i64 low = max_score, high = 0;
    
#pragma omp parallel
    {
      vector<i64>& L_histogram = L_histograms[omp_get_thread_num()];
      vector<euler_tour> L_tours_next;
      
      i64 L_low = max_score, L_high = 0;
      
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
           L_histogram.data(), L_low, L_high,
           cutoff, cutoff_keep_probability);

        #pragma omp critical
        {
          if(!increased_tree_size) {
            tree_pool.eb(tour_current);
          }else{
            delete[] tour_current.data;
          }
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
        FORU(i,L_low,L_high) histogram[i] += L_histogram[i];
        FORU(i,L_low,L_high) L_histogram[i] = 0;
        low = min(low, L_low);
        high = max(high, L_high);
      }
    }

    // if(best == 0) {
    //   debug("FOUND");
    //   return; // TODO: find solution
    // }
    
    { i64 total_count = 0;
      cutoff = max_score;
      cutoff_keep_probability = 1.0;
      FORU(i, low, high) {
        if(total_count+histogram[i] > width) {
          cutoff = i;
          cutoff_keep_probability = (float)(width-total_count) / (float)(histogram[i]);
          break;
        }
        total_count += histogram[i];
      }
      FORU(i, low, high) histogram[i] = 0;
    }

    i64 total_size = 0;
    for(auto const& t : tours_next) total_size += t.size;
    
    cerr << setw(6) << istep <<
      ": scores = " << setw(3) << low << ".." << setw(3) << cutoff <<
      ", tree size = " << setw(12) << total_size <<
      ", num trees = " << setw(4) << tours_next.size() <<
      ", elapsed = " << setw(10) << timer_s.elapsed() << "s" <<
      endl;

    tours_current = tours_next;
    
    if(low == 0) {
      vector<u8> solution;
      
#pragma omp parallel
      {
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
          
          auto lsolution = find_solution
            (P, initial_state, initial_direction,
             istep,
             tour_current);
          if(!lsolution.empty()) {
            #pragma omp critical
            {
              solution = lsolution;
            }
          }
        }
      }
      
      return solution;
    }

  }

  return {};
}

void solve(puzzle_data const& P,
           puzzle_state initial_state,
           u8 initial_directions,
           i64 width) {
  auto solution = beam_search
    (P, initial_state, initial_directions, width);
  
  vector<char> L, R;
  u8 last_direction_src = (initial_directions >> 0 & 1);
  u8 last_direction_tgt = (initial_directions >> 1 & 1); 
  for(auto m : solution) {
    if(m < 6) {
      if(last_direction_src == 0) {
        L.pb('1'+m);
      }else{
        L.pb('A'+(m+5)%6);
      }
      
      last_direction_src ^= 1;
    }else{
      m -= 6;

      if(last_direction_tgt == 0) {
        R.pb('A'+(m+2)%6);
      }else{
        R.pb('1'+(m+3)%6);
      }
      
      last_direction_tgt ^= 1;
    }
  }

  reverse(all(R));
  L.insert(end(L),all(R));

  auto cost = L.size();
  auto filename = "solutions/" + to_string(P.n) + "/" + to_string(cost); 
  
  ofstream out(filename);
  out << P.n << ":";
  for(auto c : L) {
    out << c;
  }
  out << endl;
  out.close();
}

int main(int argc, char** argv) {
  runtime_assert(argc == 4);

  i64 sz = atoi(argv[1]);
  i64 width = atoll(argv[2]);
  i32 initial_directions = atoi(argv[3]);
  debug(sz, width);
  
  init_hash();
  init_rng();
  
  auto C = load_configurations();

  runtime_assert(C.count(sz));
  unique_ptr<puzzle_data> P = make_unique<puzzle_data>();
  P->make(sz);
  solve(*P, C[sz], initial_directions, width);

  return 0;
}
