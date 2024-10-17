#include "header.hpp"
#include "puzzle.hpp"
#include <cctype>

string get_solution(string filename) {
  ifstream is(filename);
  runtime_assert(is.good());
  char c = '0';
  while(c != ':') {
    runtime_assert(is.good());
    is>>c;
  }

  string sol; is>>sol;
  string out;
  for(char c : sol) {
    if(c == ':') {
      out.clear();
    }
    if(('1' <= c && c <= '6') || ('A' <= c && c <= 'F')) {
      out.pb(c);
    }
  }
  return out;
}

int main(int argc, char** argv) {
  runtime_assert(argc == 3);

  i64 sz = atoi(argv[1]);
  
  auto C = load_configurations();

  runtime_assert(C.count(sz));
  unique_ptr<puzzle_data> puzzle_ptr = make_unique<puzzle_data>();
  puzzle_ptr->make(sz);
  runtime_assert(C.count(sz));

  puzzle_data const& puzzle = *puzzle_ptr;
  
  string filename = argv[2];
  string sol = get_solution(filename);

  if(sol.empty()) return 0;
  
  puzzle_state state = C[sz];
  u8 dir = isalpha(sol[0]) ? 1 : 0;

  for(auto c : sol) {
    u8 m;
    if(dir == 0) {
      m = c - '1';
    }else{
      m = c - 'A';
      m = (m+1)%6;
    }

    state.do_move(puzzle, m, dir);
    dir ^= 1;

    cerr << " ==================== " << endl;
    state.print(puzzle);
  }
  
  return 0;
}
