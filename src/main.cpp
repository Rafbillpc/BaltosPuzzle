#include "header.hpp"
#include "find_perms.hpp"
#include "puzzle.hpp"
#include "eval.hpp"
#include "solver.hpp"
#include <omp.h>
#include <argparse/argparse.hpp>


int main(int argc, char** argv) {
  argparse::ArgumentParser program("BaltosPuzzle");

  program.add_argument("--n")
    .scan<'i', int>();

  argparse::ArgumentParser solve_cmd("solve");
  program.add_subparser(solve_cmd);

  solve_cmd.add_argument("--width")
    .required()
    .scan<'u', u32>();

  solve_cmd.add_argument("--dir")
    .scan<'u', u32>()
    .default_value(0u);
 
  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    cerr << err.what() << endl;
    cerr << program;
    return 1;
  }
  
  auto n = program.get<int>("n");
  runtime_assert(3 <= n && n <= 27);

  find_perms();
  puzzle.make(n);
  init_eval();

  if(program.is_subcommand_used(solve_cmd)) {
    debug("here");
    u32 width = solve_cmd.get<u32>("width");
    debug(width);
    u32 dirs = solve_cmd.get<u32>("dir");
    debug(dirs);

    auto C = load_configurations();
    runtime_assert(C.count(n));
    
    solve(C[n], width, dirs);
    
  }else{
    cerr << program;
  }
    
  
  return 0;
}
