#include "solver.hpp"
#include "find_perms.hpp"
#include "beam_search.hpp"
#include "puzzle.hpp"
#include <omp.h>

struct move_seq {
  vector<u8> moves;

  bool empty() const {
    return moves.empty();
  }

  u8 back() const {
    return moves.back();
  }

  i32 size() const {
    return moves.size();
  }
  
  void do_move(u8 m) {
    if(!moves.empty() && moves.back() == (m+3)%6) moves.pop_back();
    else moves.pb(m);
  }
};

i32 overlap(move_seq const& m1, move_seq const& m2) {
  i32 i = 0;
  while(i < m1.size() && i < m2.size() &&
        m1.moves[m1.size()-1-i] == m2.moves[m2.size()-1-i])
    {
      i += 1;
    }
  return i;
}

void postpro
(puzzle_state const& initial_state,
 u8 initial_directions,
 vector<u8> const& solution)
{
  move_seq L, R;
  for(auto m : solution) {
    if(m < 6) L.do_move(m);
    else R.do_move(m-6);
  }

  puzzle_state SL; SL = initial_state; 
  puzzle_state SR; SR.set_tgt();
  SL.direction = (initial_directions >> 0) & 1;
  SR.direction = (initial_directions >> 1) & 1;

  for(auto m : L.moves) SL.do_move(m);
  for(auto m : R.moves) SR.do_move(m);

  while(1) {
    // try L
    { move_seq X = L;
      puzzle_state SX = SL;
      move_seq Y = L;
      puzzle_state SY = SL;
      while(!X.empty()) {
        auto m = X.back();
        X.do_move((m+3)%6);
        SX.do_move((m+3)%6);
        Y.do_move((m+3)%6);
        SY.do_move((m+3)%6);

        for(auto const& p : perms) {
          FOR(i, p.size) {
            X.do_move(p.data[i]);
            SX.do_move(p.data[i]);
          }

          auto cost = 0; // X.size() + L.size() + R.size() - overlap(L, X);
          auto value = 0;
          FORU(u, 1, puzzle.size-1) {
            value += puzzle.dist_eval
              [SL.tok_to_pos[SY.pos_to_tok[SX.tok_to_pos[u]]]]
              [SR.tok_to_pos[u]];
          }
          
          FORD(i, p.size-1, 0) {
            X.do_move((p.data[i]+3)%6);
            SX.do_move((p.data[i]+3)%6);
          }
        }
      }
    }
  }
  
  // while(!R.empty()) {
  //   auto m = R.back();
  //   L.do_move((m+3)%6);
  //   R.do_move((m+3)%6);
  //   SL.do_move((m+3)%6);
  //   SR.do_move((m+3)%6);
  // }

  // debug(L.size()+R.size()-overlap(L,R));
  // SL.print();
  // SR.print();
  
  // while(1) {
  //   i32 value0 = 0;
  //   FORU(u, 1, puzzle.size-1) value0 += (SL.tok_to_pos[u] == SR.tok_to_pos[u]);
  //   i32 cost0 = L.size()+R.size()-overlap(L,R);

  //   f32 best = 0;
  //   i32 count = 0;
  //   i32 best_ir = 0;
  //   perm_t best_perm;

  //   while(!L.empty()) {
  //     auto m = L.back();
  //     L.do_move((m+3)%6);
  //     R.do_move((m+3)%6);
  //     SL.do_move((m+3)%6);
  //     SR.do_move((m+3)%6);
  //   }
    
  //   i32 rsz = R.size();
  
  //   FOR(ir, rsz) {
  //     auto m = R.back();
  //     L.do_move((m+3)%6);
  //     R.do_move((m+3)%6);
  //     SL.do_move((m+3)%6);
  //     SR.do_move((m+3)%6);

  //     for(auto const& p : perms) {
  //       FOR(i, p.size) {
  //         L.do_move(p.data[i]);
  //         SL.do_move(p.data[i]);
  //       }
  //       i32 value = 0;
  //       FORU(u, 1, puzzle.size-1) value += (SL.tok_to_pos[u] == SR.tok_to_pos[u]);

  //       i32 gain = value - value0;

  //       if(1 || gain == p.modified) {
  //         i32 cost = (L.size()+R.size()-overlap(L,R)) - cost0;

  //         f32 cur;
  //         if(cost > 0) cur = 1.0 * gain / cost;
  //         else cur = 1.0 * gain * 1000 * (1-cost);

  //         if(cur > best) {
  //           best = cur;
  //           count = 0;
  //         }
  //         if(cur == best) {
  //           count += 1;
  //           if(rng.random32(count) == 0) {
  //             debug(value0, value, gain, cost, cur);
  //             best_ir   = ir;
  //             best_perm = p;
  //           }
  //         }
  //       }
          
  //       FORD(i, p.size-1, 0) {
  //         L.do_move((p.data[i]+3)%6);
  //         SL.do_move((p.data[i]+3)%6);
  //       }
  //     }
  //   }

  //   while(!L.empty()) {
  //     auto m = L.back();
  //     L.do_move((m+3)%6);
  //     R.do_move((m+3)%6);
  //     SL.do_move((m+3)%6);
  //     SR.do_move((m+3)%6);
  //   }
    
  //   debug(best, count);

  //   if(best <= 0) break;
    
  //   runtime_assert(R.size() == rsz);
  //   FOR(ir, rsz) {
  //     auto m = R.back();
  //     L.do_move((m+3)%6);
  //     R.do_move((m+3)%6);
  //     SL.do_move((m+3)%6);
  //     SR.do_move((m+3)%6);

  //     if(ir == best_ir) {
  //       FOR(i, best_perm.size) {
  //         L.do_move(best_perm.data[i]);
  //         SL.do_move(best_perm.data[i]);
  //       }
  //     }
  //   }

  //   while(!R.empty()) {
  //     auto m = R.back();
  //     L.do_move((m+3)%6);
  //     R.do_move((m+3)%6);
  //     SL.do_move((m+3)%6);
  //     SR.do_move((m+3)%6);
  //   }
    
  //   debug(L.size()+R.size()-overlap(L,R));
  //   SL.print();
  //   SR.print();
  // }
}

void save_solution
(u32 initial_directions,
 vector<u8> const& solution)
{
  vector<char> L, R;
  u8 last_direction_src = (initial_directions >> 0) & 1;
  u8 last_direction_tgt = (initial_directions >> 1) & 1; 
  for(auto m : solution) {
    if(m < 6) {
      if(last_direction_src == 0) {
        L.pb('1'+(m+1)%6);
      }else{
        L.pb('A'+(m+6)%6);
      }
      
      last_direction_src ^= 1;
    }else{
      m -= 6;

      if(last_direction_tgt == 0) {
        R.pb('A'+(m+3)%6);
      }else{
        R.pb('1'+(m+4)%6);
      }
      
      last_direction_tgt ^= 1;
    }
  }

  reverse(all(R));
  L.insert(end(L),all(R));

  auto cost = L.size();
  auto filename = "solutions/" + to_string(puzzle.n) + "/" + to_string(cost); 
  
  ofstream out(filename);
  out << puzzle.n << ":";
  for(auto c : L) {
    out << c;
  }
  out << endl;
  out.close();
}

void solve
(puzzle_state const& initial_state,
 u32 width,
 u32 dirs) {

  auto search = make_unique<beam_search>(beam_search_config {
      .print = true,
      .print_interval = 1,
      .width = width,
      .num_threads = (u32)omp_get_max_threads()
    });

  beam_state state;
  state.src = initial_state;
  state.src.direction = (dirs >> 0) & 1;
  state.tgt.set_tgt();
  state.tgt.direction = (dirs >> 1) & 1;
  state.init();
  
  auto result = search->search(state);
  postpro(initial_state, dirs, result.solution);
  
  // save_solution(dirs, result.solution);
}

