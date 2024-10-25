#include "training.hpp"
#include "beam_search.hpp"
#include "eval.hpp"
#include <omp.h>

vector<training_sample> gather_samples() {
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
          .print = true,
          .print_interval = 1000,
          .width = 1<<9,
          .features_save_probability = 0.002,
        });
    }

    beam_search &search = *searches[thread_id];

    while(1) {
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
      bool should_stop = 0;
      
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

        if(samples.size() > 1'000'000) should_stop = 1;
      }

      if(should_stop) break;
    }

    #pragma omp critical
    {
      for(auto &s : searches) if(s) s->should_stop = true;
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

void update_weights(vector<training_sample> const& samples) {
  vector<f64> w(NUM_FEATURES, 0.0);
  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    w[puzzle.dist_feature[u][v]] = (f64)weights.dist_weight[u][v] / 64.0;
  }
  FOR(m, bit(7)) {
    w[nei_feature_key[m]] = (f64)weights.nei_weight[m] / 64.0;
  }
  
  vector<f64> m(NUM_FEATURES, 0.0);
  vector<f64> v(NUM_FEATURES, 0.0);
  f64 alpha0 = 1;
  f64 eps = 1e-8;
  f64 beta1 = 0.9, beta2 = 0.999;
  f64 lambda = 1e-6;
 
  i32 time = 0;
  while(1) {
    time += 1;

    f64 alpha = alpha0 * pow(0.995, time);

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
        value = tanh(value);

        f64 der = 1.0 - value * value;

        L_total_loss += value;

        FOR(i, NUM_FEATURES) if(i != 0) {
          L_g[i] += (sample.features1[i] - sample.features2[i]) * der;
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
    FOR(i, NUM_FEATURES) if(i != 0) {
      m[i] = beta1 * m[i] + (1 - beta1) * g[i];
      v[i] = beta2 * v[i] + (1 - beta2) * g[i] * g[i];

      f64 delta = alpha * m[i] / (sqrt(v[i]) + eps);
      w[i] -= delta;
      max_delta = max(max_delta, abs(delta));
    }

    FOR(i, NUM_FEATURES) {
      w[i] = max(w[i], 0.0);
    }
    
    // printing
    if(time % 100 == 0) {
      cerr
        << "time = " << setw(4) << time
        << ", lr = " << fixed << setprecision(6) << alpha
        << ", loss = " << fixed << setprecision(6) << total_loss
        << ", max = " << fixed << setprecision(6) << max_delta
        << endl;
    }

    if(max_delta < 1e-4) break;
  }
  
  runtime_assert(w[1] > 0.01);
  {
    cerr << "TRI: " << endl;
    FOR(u, puzzle.n) {
      FOR(v, u+1) if(u+v < puzzle.n) {
        cerr << setw(5) << setprecision(2) << fixed
             << w[dist_feature_key[u][v]] / w[1] << " ";
      }
      cerr << endl;
    }
    FOR(i, NUM_FEATURES_NEI) {
      cerr << setw(5) << setprecision(2) << fixed
           << w[NUM_FEATURES_DIST + i] / w[1] << " ";
    }
    cerr << endl;
  }

  FOR(u, puzzle.size) FOR(v, puzzle.size) {
    weights.dist_weight[u][v] = 64 * (w[puzzle.dist_feature[u][v]] / w[1]);
  }
  FOR(u, bit(7)) {
    weights.nei_weight[u] = 64 * (w[nei_feature_key[u]] / w[1]);
  }
}

void training_loop() {
  while(1) {
    auto samples = gather_samples();
    update_weights(samples);
  }
}
