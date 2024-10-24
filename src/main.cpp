#include "header.hpp"
#include "beam_search.hpp"
#include "puzzle.hpp"
#include <omp.h>

struct training_sample {
  vector<i32> features1;
  vector<i32> features2;
};

vector<training_sample> gather_samples() {
  vector<training_sample> samples;

  i64 total_size = 0;
  i64 total_count = 0;
  
#pragma omp parallel
  {
    unique_ptr<beam_search> search = make_unique<beam_search>(beam_search_config {
        .print = false,
        .width = 1<<12,
      });

    while(1) {
      u64 seed;
#pragma omp critical
      {
        seed = rng.randomInt64();
      }
    
      puzzle_state src;
      src.generate(seed);

      beam_state state;
      state.src = src;
      state.src.direction = rng.random32(2);
      state.tgt.set_tgt();
      state.tgt.direction = rng.random32(2);
      state.init();
    
      auto result = search->search(state);
      if(result.solution.empty()) continue;
      bool should_stop = 0;
      
#pragma omp critical
      {
        auto size = result.solution.size();

        total_size += size;
        total_count += 1;
        
        FOR(i, size) {
          samples.pb(training_sample {
              .features1 = state.features(),
              .features2 = result.features[i]
            });
          if(samples.size() % 25'000 == 0) debug(samples.size());
          state.do_move(result.solution[i]);
        }

        if(samples.size() > 500'000) should_stop = 1;
      }

      if(should_stop) break;
    }
  }

  cerr
    << "average_size = " << setprecision(10) << fixed << (f64)total_size / total_count
    << endl;
  
  return samples;
}

vector<f64> w;
void train(vector<training_sample> const& samples) {
  i32 num_features = puzzle.num_features;
  if(w.empty()) {
    w.resize(num_features);
    FOR(i, num_features) w[i] = rng.randomDouble();
    w[0] = 0;
  }
  
  vector<f64> m(num_features, 0.0);
  vector<f64> v(num_features, 0.0);
  f64 alpha0 = 1e-1;
  f64 eps = 1e-8;
  f64 beta1 = 0.9, beta2 = 0.999;
  f64 lambda = 1e-5;

  i32 time = 0;
  while(1) {
    time += 1;

    f64 alpha = alpha0 * pow(0.99, time);

    // compute gradients
    vector<f64> g(num_features);
    f64 total_loss = 0;
    for(auto const& sample : samples) {
      f64 value = 0.0;
      FOR(i, num_features) {
        value += (sample.features1[i] - sample.features2[i]) * w[i];
      }
      value = tanh(value);

      f64 der = 1.0 - value * value;

      total_loss += value;

      FOR(i, num_features) if(i != 0) {
        g[i] += (sample.features1[i] - sample.features2[i]) * der;
      }
    }
    FOR(i, num_features) g[i] /= samples.size();

    // L2-reg
    FOR(i, num_features) {
      g[i] += lambda * w[i];
    }
    
    
    f64 max_delta = 0;
    
    // update the weights
    FOR(i, num_features) if(i != 0) {
      m[i] = beta1 * m[i] + (1 - beta1) * g[i];
      v[i] = beta2 * v[i] + (1 - beta2) * g[i] * g[i];

      f64 delta = alpha * m[i] / (sqrt(v[i]) + eps);
      w[i] -= delta;
      max_delta = max(max_delta, abs(delta));
    }

    // printing
    if(time % 100 == 0) {
      cerr
        << "time = " << setw(4) << time
        << ", lr = " << fixed << setprecision(6) << alpha
        << ", loss = " << fixed << setprecision(6) << total_loss / samples.size()
        << ", max = " << fixed << setprecision(6) << max_delta
        << endl;
    }

    if(max_delta < 1e-4) break;
  }

  runtime_assert(w[1] > 0.01);
  {
    cerr << "HEX: " << endl;
    u32 ix = 0;
    FOR(u, 2*puzzle.n-1) {
      u32 ncol = 2*puzzle.n-1 - abs(u-(puzzle.n-1));
      FOR(i, abs(u-(puzzle.n-1))) cerr << "     ";
      FOR(icol, ncol) {
        cerr << setw(5) << setprecision(2) << fixed <<
          w[puzzle.dist_key[puzzle.center][ix]] / w[1] << "     ";
        ix += 1;
      }
      cerr << endl;
    }

    cerr << "TRI: " << endl;
    FOR(u, puzzle.n) {
      FOR(v, u+1) if(u+v < puzzle.n) {
        cerr << setw(5) << setprecision(2) << fixed
             << w[u*(u+1)/2 + v] / w[1] << " ";
      }
      cerr << endl;
    }
  }
  
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    weights.dist_weight[u][v] = 100 * (w[puzzle.dist_key[u][v]] / w[1]);
  }
}

int main(int argc, char** argv) {
  puzzle.make(8);

  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    weights.dist_weight[u][v] = 10 * puzzle.dist[u][v];
  }

  while(1) {
    auto samples = gather_samples();
    train(samples);
  }
  
  return 0;
}
