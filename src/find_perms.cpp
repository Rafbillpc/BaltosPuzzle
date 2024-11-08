#include "find_perms.hpp"
#include "puzzle.hpp"

void find_perms() {
  puzzle.make(10);

  puzzle_state S; S.set_tgt();
  i64 count = 0;

  set<vector<array<i32, 2>>> seen;
  perm_t perm;
  
  auto bt = [&](auto self, i32 i, u8 last, i32 limit) -> void {
    if(i == limit) {
      if(S.pos_to_tok[puzzle.center] == 0) {
        i32 sz = 0;
        vector<array<i32, 2>> P;
        
        FOR(u, puzzle.size) if((i32)S.pos_to_tok[u] != puzzle.tgt_pos_to_tok[u]) {
          P.pb({u, (i32)S.pos_to_tok[u]});
          sz += 1;
        }
        if(!seen.count(P) && ((limit == 4 && sz == 4))) {
          count += 1;
          seen.insert(P);
          perm.modified = sz;
          perms.pb(perm);
        }
      }
      return;
    }
    FOR(m, 6) if((m+3)%6 != last) {
      S.do_move(m);
      perm.data[i] = m;
      perm.size = i+1;
      self(self, i+1, m, limit);
      S.do_move((m+3)%6);
    }
  };
  for(auto limit : {4,6,8}) {
    bt(bt,0,6,limit);
  }

  debug(perms.size());
}
