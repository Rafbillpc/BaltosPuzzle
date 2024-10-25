#include "header.hpp"
#include "puzzle.hpp"
#include "training.hpp"
#include <omp.h>
#include <argparse/argparse.hpp>


int main(int argc, char** argv) {
  argparse::ArgumentParser program("BaltosPuzzle");

  program.add_argument("--n")
    .scan<'i', int>();

  argparse::ArgumentParser train_cmd("train");
  program.add_subparser(train_cmd);
  
  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    cerr << err.what() << endl;
    cerr << program;
    return 1;
  }
  
  auto n = program.get<int>("n");
  runtime_assert(3 <= n && n <= 27);
  
  init_eval();
  puzzle.make(n);
  weights.init();

  if(program.is_subcommand_used(train_cmd)) {
    training_loop();
  }else{
    cerr << program;
  }
    
  
  return 0;
}
