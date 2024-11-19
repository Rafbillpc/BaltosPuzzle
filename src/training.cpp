#include "training.hpp"
#include "beam_search.hpp"
#include "eval.hpp"
#include <omp.h>

const i32 BATCH_SIZE = 1<<16;

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
          .num_threads = 1,
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

      u64 size;
      u64 from;
#pragma omp critical
      {
        size = result.solution.size();
        from = samples.size();
        if(size > 0) {
          total_size += size;
          total_size2 += (f64)size * size;
          total_count += 1;
 
          samples.resize(from + result.saved_features.size());

          if(samples.size() / 100'000 != from / 100'000) debug(samples.size());

          i32 curi = 0;
          for(auto [i,v] : result.saved_features) {
            while(curi < i) {
              state.do_move(result.solution[curi]);
              curi += 1;
            }
            features_vec v1;
            state.features(v1);
            samples[from] = training_sample(v1, v);
            from += 1;
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
  FOR(i, NUM_FEATURES) w[i] = 10 * rng.randomDouble();
  
  vector<f64> m(NUM_FEATURES, 0.0);
  vector<f64> v(NUM_FEATURES, 0.0);
  f64 alpha0 = 1;
  f64 eps = 1e-8;
  f64 beta1 = 0.9, beta2 = 0.999;
  f64 lambda = 1e-7;
  
  vector<training_sample> batch_samples(BATCH_SIZE);
  
  FOR(iter, config.training_iters) {
    f64 alpha = alpha0 * pow(1e-1, 1.0 * iter / config.training_iters);

#pragma omp parallel for schedule(static, 512)
    FOR(i, BATCH_SIZE) {
      batch_samples[i] = rng.sample(samples);
    }
    
    // compute gradients
    vector<f64> g(NUM_FEATURES);
    f64 total_loss = 0;
#pragma omp parallel
    {
      vector<f64> L_g(NUM_FEATURES);
      f64 L_total_loss = 0;

#pragma omp for
      FOR(isample, BATCH_SIZE) {
        auto const& sample = batch_samples[isample];
        f64 value = 0.0;
        FOR(i, NUM_FEATURES) {
          value += sample.features[i] * w[i];
        }
        value = tanh(value);
        f64 derivative = 1-value*value;

        L_total_loss += value;

        FOR(i, NUM_FEATURES) {
          L_g[i] += sample.features[i] * derivative;
        }
      }

#pragma omp critical
      {
        FOR(i, NUM_FEATURES) g[i] += L_g[i];
        total_loss += L_total_loss;
      }
    }
    total_loss /= BATCH_SIZE;
    FOR(i, NUM_FEATURES) g[i] /= BATCH_SIZE;

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
    w[dist_feature_key[0][0]] = 0;
    w[nei_feature_key[0]] = 0;

    FOR(u, puzzle.n) {
      FOR(v, u+1) if(u+v < puzzle.n) {
        if(u > 0 && v < u) w[dist_feature_key[u][v]] = max(w[dist_feature_key[u][v]], w[dist_feature_key[u-1][v]]);
        if(v > 0) w[dist_feature_key[u][v]] = max(w[dist_feature_key[u][v]], w[dist_feature_key[u][v-1]]);
      }
    }
    
    // printing
    if(iter % 100 == 99) {
      cerr
        << "time = " << setw(4) << (iter+1)
        << ", lr = " << fixed << setprecision(6) << alpha
        << ", loss = " << fixed << setprecision(6) << total_loss
        << ", max = " << fixed << setprecision(6) << max_delta
        << endl;
    }
  }
 
  {
    cerr << "Values" << endl;
    cerr << "DIST:" << endl;
    FOR(u, puzzle.n) {
      FOR(v, u+1) if(u+v < puzzle.n) {
        cerr << setw(5) << setprecision(2) << fixed
             << w[dist_feature_key[u][v]] << " ";
      }
      cerr << endl;
    }
    cerr << "NEI:" << endl;
    FOR(u, bit(6)) {
        cerr << setw(5) << setprecision(2) << fixed
             << w[nei_feature_key[u]] << " ";
    }
    cerr << "CYCLE2:" << setw(5) << setprecision(2) << fixed
         << w[cycle2_feature_key] << endl;
    cerr << "CYCLE3:" << setw(5) << setprecision(2) << fixed
         << w[cycle3_feature_key] << endl;
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
  FOR(step, config.steps) {
    auto samples = gather_samples(config);
    runtime_assert(samples.size() > BATCH_SIZE);
    update_weights(config, samples);
  }
}
