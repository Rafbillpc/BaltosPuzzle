#include "header.hpp"
#include "puzzle.hpp"
#include "training.hpp"
#include <fstream>
#include <omp.h>
#include <argparse/argparse.hpp>


int main(int argc, char** argv) {
  argparse::ArgumentParser program("BaltosPuzzle");

  program.add_argument("--n")
    .scan<'i', int>();

  program.add_argument("--load")
    .default_value("");
  
  argparse::ArgumentParser train_cmd("train");
  program.add_subparser(train_cmd);

  train_cmd.add_argument("--width")
    .scan<'u', u32>()
    .default_value(1<<10);

  train_cmd.add_argument("--count")
    .scan<'u', u32>()
    .default_value(1'000'000);

  train_cmd.add_argument("--ratio")
    .scan<'f', f32>()
    .default_value(0.001);

  train_cmd.add_argument("--print")
    .default_value(false)
    .implicit_value(true);

  train_cmd.add_argument("-o", "--output")
    .default_value("");
  
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

  string load_weights = program.get("load");
  if(!load_weights.empty()) {
    ifstream is(load_weights);
    runtime_assert(is.good());
    weights_vec w;
    is.read((char*)&w, sizeof(w));
    weights.from_weights(w);
  }
  
  if(program.is_subcommand_used(train_cmd)) {

    bool print = train_cmd.get<bool>("print");
    u32 width = train_cmd.get<u32>("width");
    u32 count = train_cmd.get<u32>("count");
    f32 ratio = train_cmd.get<f32>("ratio");

    string output = train_cmd.get("output");
    
    auto config = training_config {
      .print = print,
      .gather_width = width,
      .gather_count = count,
      .features_save_probability = ratio,
      .output = output,
    };
    
    training_loop(config);

  }else{
    cerr << program;
  }
    
  
  return 0;
}
