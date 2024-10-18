#include "header.hpp"
#include "puzzle.hpp"

// vector<vector<i32>> seqs[2];

// void find_perms() {
//   unique_ptr<puzzle_data> puzzle_ptr = make_unique<puzzle_data>();
//   auto& puzzle = *puzzle_ptr;
//   puzzle.make(21);
  
//   FORU(depth, 1, 10) if(depth%2 == 0) FOR(d, 2) {

//     u8 last_direction = d;

//     puzzle_state base_state;
//     base_state.reset(puzzle);
    
//     puzzle_state S;
//     S.reset(puzzle);

//     i64 count = 0;
//     map<int,int> M;

//     vector<i32> X;
//     auto bt = [&](auto self, i32 i, u8 last_m){
//       if(i == depth){
//         if(S.tok_to_pos[0] == base_state.tok_to_pos[0]) {
//           i32 nchanged = 0;
//           FORU(i, 1, puzzle.size-1) if(S.tok_to_pos[i] != base_state.tok_to_pos[i]) {
//             nchanged += 1;
//           }
//           if(nchanged > 0 && nchanged <= 4) {
//             count += 1;
//             M[nchanged] += 1;
//             seqs[d].pb(X);
//           }
//         }
        
//         return;
//       }
//       FOR(m, 6) if((m+3)%6 != last_m) {
//         X.pb(m);
//         S.do_move(puzzle, m, last_direction);
//         self(self, i+1, m);
//         S.undo_move(puzzle, m, last_direction);
//         X.pop_back();
//       }
//     };

//     bt(bt, 0, 6);
//     debug(depth, d, count);
//     debug(M);
//   }
// }

// struct move_seq {
//   vector<i32> seq;

//   i32 size() const {
//     return seq.size();
//   }
  
//   void push(i32 m) {
//     if(seq.empty() || seq.back() != (m+3)%6) {
//       seq.push_back(m);
//     }else{
//       seq.pop_back();
//     }
//   }

//   i32 peek() const { return seq.back(); }

//   i32 peek_n(i32 x) const { return seq[seq.size()-1-x]; }
  
//   void pop() {
//     seq.pop_back();
//   }
// };

// struct state {
//   puzzle_state src_state;
//   puzzle_state tgt_state;
//   u8 src_direction, tgt_direction;
  
//   move_seq L, R;

//   u32 total_distance;

//   i32 cost() const {
//     return L.size() + R.size() - get_overlap();
//   }

//   void init_distance(puzzle_data const& puzzle) {
//     total_distance = 0;
//     FOR(i, puzzle.size) {
//       total_distance
//         += puzzle.dist_tok[src_state.pos_to_tok[i]][tgt_state.pos_to_tok[i]];
//     }
//   }
  
//   void move_L(puzzle_data const& puzzle, u8 move) {
//     L.push(move);
    
//     u32 a = src_state.tok_to_pos[0], b, c;
    
//     if(src_direction == 0) {
//       b = puzzle.data[a].data[move];
//       c = puzzle.data[a].data[(move+1)%6];
//     }else{
//       b = puzzle.data[a].data[move];
//       c = puzzle.data[a].data[(move+5)%6];
//     }

//     u32 xb = src_state.pos_to_tok[b];
//     u32 xc = src_state.pos_to_tok[c];

//     total_distance -= puzzle.dist_tok[src_state.pos_to_tok[a]][tgt_state.pos_to_tok[a]];
//     total_distance -= puzzle.dist_tok[src_state.pos_to_tok[b]][tgt_state.pos_to_tok[b]];
//     total_distance -= puzzle.dist_tok[src_state.pos_to_tok[c]][tgt_state.pos_to_tok[c]];

//     src_state.pos_to_tok[a]  = xc;
//     src_state.pos_to_tok[b]  = 0;
//     src_state.pos_to_tok[c]  = xb;
//     src_state.tok_to_pos[0]  = b;
//     src_state.tok_to_pos[xb] = c;
//     src_state.tok_to_pos[xc] = a;

//     total_distance += puzzle.dist_tok[src_state.pos_to_tok[a]][tgt_state.pos_to_tok[a]];
//     total_distance += puzzle.dist_tok[src_state.pos_to_tok[b]][tgt_state.pos_to_tok[b]];
//     total_distance += puzzle.dist_tok[src_state.pos_to_tok[c]][tgt_state.pos_to_tok[c]];

//     src_direction ^= 1;
//   }
  
//   void move_R(puzzle_data const& puzzle, u8 move) {
//     R.push(move);
    
//     u32 a = tgt_state.tok_to_pos[0], b, c;
      
//     if(tgt_direction == 0) {
//       b = puzzle.data[a].data[move];
//       c = puzzle.data[a].data[(move+1)%6];
//     }else{
//       b = puzzle.data[a].data[move];
//       c = puzzle.data[a].data[(move+5)%6];
//     }

//     u32 xb = tgt_state.pos_to_tok[b];
//     u32 xc = tgt_state.pos_to_tok[c];

//     total_distance -= puzzle.dist_tok[src_state.pos_to_tok[a]][tgt_state.pos_to_tok[a]];
//     total_distance -= puzzle.dist_tok[src_state.pos_to_tok[b]][tgt_state.pos_to_tok[b]];
//     total_distance -= puzzle.dist_tok[src_state.pos_to_tok[c]][tgt_state.pos_to_tok[c]];
  
//     tgt_state.pos_to_tok[a]  = xc;
//     tgt_state.pos_to_tok[b]  = 0;
//     tgt_state.pos_to_tok[c]  = xb;
//     tgt_state.tok_to_pos[0]  = b;
//     tgt_state.tok_to_pos[xb] = c;
//     tgt_state.tok_to_pos[xc] = a;

//     total_distance += puzzle.dist_tok[src_state.pos_to_tok[a]][tgt_state.pos_to_tok[a]];
//     total_distance += puzzle.dist_tok[src_state.pos_to_tok[b]][tgt_state.pos_to_tok[b]];
//     total_distance += puzzle.dist_tok[src_state.pos_to_tok[c]][tgt_state.pos_to_tok[c]];
      
//     tgt_direction ^= 1;
//   }

//   void shift_L(puzzle_data const& puzzle) {
//     u8 m = L.peek(); 
//     move_L(puzzle, (m+3)%6);
//     move_R(puzzle, (m+3)%6);
//   }
 
//   void shift_R(puzzle_data const& puzzle) {
//     u8 m = R.peek(); 
//     move_L(puzzle, (m+3)%6);
//     move_R(puzzle, (m+3)%6);
//   }

//   void reset_L(puzzle_data const& puzzle) {
//     while(L.size()) {
//       shift_L(puzzle);
//     }
//   }

//   void reset_R(puzzle_data const& puzzle) {
//     while(R.size()) {
//       shift_R(puzzle);
//     }
//   }

  
//   i32 get_overlap() const {
//     i32 sz = min(L.size(), R.size());
//     FOR(i, sz) if(L.peek_n(i) != R.peek_n(i)) return i;
//     return sz;
//   }
// };

// void solve(puzzle_data const& puzzle,
//            puzzle_state const& initial_state,
//            u8 initial_direction) {
//   state S;
//   S.src_state = initial_state;
//   S.tgt_state.reset(puzzle);
//   S.src_direction = initial_direction;
//   S.init_distance(puzzle);
  
//   while(S.src_state.tok_to_pos[0] != S.tgt_state.tok_to_pos[0]) {
//     u8 m = rng.random32(6);
//     S.move_L(puzzle, m);
//   }

//   S.tgt_direction = S.src_direction;

//   while(1) {
//     S.reset_L(puzzle);
//     i32 steps = S.R.size()+1;
//     S.init_distance(puzzle);

//     f32 best_value = 0;
//     vector<i32> best_seq;
//     i32 best_step;
//     i32 new_distance;
    
//     FOR(step, steps) {
//       if(step != 0) S.shift_R(puzzle);
//       auto d = S.total_distance;
//       auto c = S.cost();

//       for(auto const& seq : seqs[S.src_direction]) {
//         i32 sz = seq.size();
//         FOR(i, sz) S.move_L(puzzle, seq[i]);
//         if(S.total_distance < d){
//           i32 gain = d - S.total_distance;
//           f32 cost = S.cost() - c;
//           f32 value = gain / max(cost, 0.001f);
//           if(value > best_value) {
//             new_distance = S.total_distance;
//             best_value = value;
//             best_seq = seq;
//             best_step = step;
//           }
//         }
//         FORD(i, sz-1, 0) S.move_L(puzzle, (seq[i]+3)%6);
//       }
//     }

//     debug(best_value, S.total_distance);
    
//     if(best_seq.empty()) break;

//     S.reset_L(puzzle);
//     FOR(step, best_step) S.shift_R(puzzle);
//     for(auto m : best_seq) S.move_L(puzzle, m);

//     runtime_assert(S.total_distance == new_distance);
//     debug(S.total_distance, S.cost());
//   }
  
//   S.reset_R(puzzle);
//   S.src_state.print(puzzle);
//   S.tgt_state.print(puzzle);
// }

int main(int argc, char** argv) {
  // runtime_assert(argc == 3);

  // i64 sz = atoi(argv[1]);
  // i32 initial_direction = atoi(argv[2]);
  
  // automaton::init();
  // auto C = load_configurations();

  // runtime_assert(C.count(sz));
  // unique_ptr<puzzle_data> puzzle_ptr = make_unique<puzzle_data>();
  // puzzle_ptr->make(sz);
  // runtime_assert(C.count(sz));
  
  // find_perms();
  // solve(*puzzle_ptr, C[sz], initial_direction);
  
  return 0;
}
