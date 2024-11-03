#include "training.hpp"
#include "beam_search.hpp"
#include "eval.hpp"
#include <fstream>
#include <omp.h>

f64 sigmoid(f64 z) {
  return 1.0 / (1.0 + std::exp(-z));
}

vector<training_sample> gather_samples(training_config const& config) {
  vector<training_sample> samples;

  i64 total_count = 0;
  f64 total_size = 0;
  f64 total_size2 = 0;

  i64 base_seed = rng.randomInt64();

  i32 num_threads = omp_get_max_threads();
  vector<unique_ptr<beam_search>> searches(num_threads);

#pragma omp parallel
  {
    auto thread_id = omp_get_thread_num();

#pragma omp critical
    {
      searches[thread_id] = make_unique<beam_search>(beam_search_config {
          .print = config.print,
          .print_interval = 1000,
          .width = config.gather_width,
          .features_save_probability = config.features_save_probability,
          .num_threads = 1
        });
    }
  }

  while(samples.size() < config.gather_count) {
    
#pragma omp parallel
    {
      auto thread_id = omp_get_thread_num();

      beam_search &search = *searches[thread_id];

      u64 seed;
#pragma omp critical
      {
        base_seed += 1;
        seed = base_seed;
      }
    
      puzzle_state src;
      src.generate(seed);

      beam_state state;
      state.src = src;
      state.src.direction = rng.random32(2);
      state.tgt.set_tgt();
      state.tgt.direction = rng.random32(2);
      state.init();
    
      auto result = search.search(state);
      
#pragma omp critical
      {
        auto size = result.solution.size();

        if(size > 0) {
          total_size += size;
          total_size2 += (f64)size * size;
          total_count += 1;

          i32 curi = 0;
          for(auto [i,v] : result.saved_features) {
            while(curi < i) {
              state.do_move(result.solution[curi]);
              curi += 1;
            }
            training_sample sample;
            state.features(sample.features1);
            sample.features2 = v;
            samples.pb(sample);
            if(samples.size() % 10'000 == 0) debug(samples.size());
          }
        }
      }

    }
  }
  
  {
    f64 mean  = total_size / total_count;
    f64 mean2 = total_size2 / total_count;
    f64 var = mean2 - mean*mean;
    f64 std = sqrt(var);
  
    cerr
      << "sizes = " << setprecision(6) << fixed << mean << " ± " << std
      << endl;
  }
  
  return samples;
}

void update_weights(training_config const& config,
                    vector<training_sample> const& samples)
{
  weights_vec w;
  FOR(i, NUM_FEATURES) w[i] = rng.randomDouble();
  
  vector<f64> m(NUM_FEATURES, 0.0);
  vector<f64> v(NUM_FEATURES, 0.0);
  f64 alpha0 = 1;
  f64 eps = 1e-8;
  f64 beta1 = 0.9, beta2 = 0.999;
  f64 lambda = 1e-6;
 
  i32 time = 0;
  while(1) {
    time += 1;

    f64 alpha = alpha0 * pow(0.99, time);

    // compute gradients
    vector<f64> g(NUM_FEATURES);
    f64 total_loss = 0;
#pragma omp parallel
    {
      vector<f64> L_g(NUM_FEATURES);
      f64 L_total_loss = 0;

#pragma omp for
      FOR(isample, samples.size()) {
        auto const& sample = samples[isample];
        f64 value = 0.0;
        FOR(i, NUM_FEATURES) {
          value += (sample.features1[i] - sample.features2[i]) * w[i];
        }
        value = sigmoid(value);
        f64 derivative = value;

        L_total_loss += -log(value);

        FOR(i, NUM_FEATURES) {
          L_g[i] += (sample.features1[i] - sample.features2[i]) * derivative;
        }
      }

#pragma omp critical
      {
        FOR(i, NUM_FEATURES) g[i] += L_g[i];
        total_loss += L_total_loss;
      }
    }
    total_loss /= samples.size();
    FOR(i, NUM_FEATURES) g[i] /= samples.size();

    // L2-reg
    FOR(i, NUM_FEATURES) {
      total_loss += (lambda/2) * w[i]*w[i];
      g[i] += lambda * w[i];
    }

    f64 max_delta = 0;
    
    // update the weights
    FOR(i, NUM_FEATURES) {
      m[i] = beta1 * m[i] + (1 - beta1) * g[i];
      v[i] = beta2 * v[i] + (1 - beta2) * g[i] * g[i];

      f64 delta = alpha * m[i] / (sqrt(v[i]) + eps);
      w[i] -= delta;
      max_delta = max(max_delta, abs(delta));
    }
    FOR(i, NUM_FEATURES) w[i] = max(w[i], 0.0);
    w[0] = 0;
    w[1] = 1;
    
    // printing
    if(time < 10 || time % 10 == 0) {
      cerr
        << "time = " << setw(4) << time
        << ", lr = " << fixed << setprecision(6) << alpha
        << ", loss = " << fixed << setprecision(6) << total_loss
        << ", max = " << fixed << setprecision(6) << max_delta
        << endl;
    }

    if(max_delta < 1e-3) break;
  }

  f64 min_weight = w[0];
  FOR(i, NUM_FEATURES) min_weight = min(min_weight, w[i]);
  FOR(i, NUM_FEATURES) w[i] -= min_weight;
  
  {
    cerr << "TRI: " << endl;
    FOR(u, puzzle.n) {
      FOR(v, u+1) if(u+v < puzzle.n) {
        cerr << setw(6) << setprecision(3) << fixed
             << w[dist_feature_key[u][v]] << " ";
      }
      cerr << endl;
    }
    FOR(i, NUM_FEATURES_NEI) {
      cerr << setw(6) << setprecision(3) << fixed
           << w[NUM_FEATURES_DIST + i] << " ";
    }
    cerr << endl;
  }

  if(!config.output.empty()) {
    ofstream os(config.output);
    runtime_assert(os.good());
    os.write((char*)&w, sizeof(weights_vec));
  }

  weights.from_weights(w);
}

void training_loop(training_config const& config) {
  while(1) {
    auto samples = gather_samples(config);
    update_weights(config, samples);
  }
}
