/*
  Copyright (c) by respective owners including Yahoo!, Microsoft, and
  individual contributors. All rights reserved.  Released under a BSD
  license as described in the file LICENSE.
*/
#pragma once
#ifdef __FreeBSD__
#include <sys/socket.h>
#endif

#include "parse_regressor.h"
#include "constant.h"
#include "interactions.h"
#include "array_parameters.h"

namespace GD
{
LEARNER::base_learner* setup(vw& all);

struct gd;

float finalize_prediction(shared_data* sd, float ret);
void print_audit_features(vw&, example& ec);
void save_load_regressor(vw& all, io_buf& model_file, bool read, bool text);
void save_load_online_state(vw& all, io_buf& model_file, bool read, bool text, GD::gd *g = nullptr);

struct multipredict_info { size_t count; size_t step; polyprediction* pred; weight_parameters& weights; /* & for l1: */ float gravity; };

inline void vec_add_multipredict(multipredict_info& mp, const float fx, uint64_t fi)
{ if ((-1e-10 < fx) && (fx < 1e-10)) return;
  weight_parameters& w = mp.weights;
  uint64_t mask = w.mask(); 
  polyprediction* p = mp.pred;
  fi &= mask;
  uint64_t top = fi + (uint64_t)((mp.count-1) * mp.step);
  uint64_t i = 0;
  if (top <= mask)
  {
	  i += fi;
	  for (; i <= top; i+= mp.step, ++p)
		  p->scalar += fx * w[i]; //TODO: figure out how to use weight_parameters::iterator (not using change_begin())
  }
  else    // TODO: this could be faster by unrolling into two loops
    for (size_t c=0; c<mp.count; ++c, fi += (uint64_t)mp.step, ++p)
    { fi &= mask;
      p->scalar += fx * w[fi]; 
    }
}

// iterate through one namespace (or its part), callback function T(some_data_R, feature_value_x, feature_weight)
template <class R, void (*T)(R&, const float, float&)>
inline void foreach_feature(weight_parameters& weights, features& fs, R& dat, uint64_t offset = 0, float mult = 1.)
{
  for (features::iterator& f : fs)
    T(dat, mult*f.value(), weights[(f.index() + offset)]);
}

// iterate through one namespace (or its part), callback function T(some_data_R, feature_value_x, feature_index)
template <class R, void (*T)(R&, float, uint64_t)>
void foreach_feature(weight_parameters& /*weights*/, features& fs, R&dat, uint64_t offset = 0, float mult = 1.)
{
  for (features::iterator& f : fs)
    T(dat, mult*f.value(), f.index() + offset);
}

// iterate through all namespaces and quadratic&cubic features, callback function T(some_data_R, feature_value_x, S)
// where S is EITHER float& feature_weight OR uint64_t feature_index
template <class R, class S, void (*T)(R&, float, S)>
inline void foreach_feature(vw& all, example& ec, R& dat)
{ uint64_t offset = ec.ft_offset;

for (features& f : ec)
    foreach_feature<R,T>(all.weights, f, dat, offset);

  INTERACTIONS::generate_interactions<R,S,T>(all, ec, dat);
}

// iterate through all namespaces and quadratic&cubic features, callback function T(some_data_R, feature_value_x, feature_weight)
template <class R, void (*T)(R&, float, float&)>
inline void foreach_feature(vw& all, example& ec, R& dat)
{ foreach_feature<R,float&,T>(all, ec, dat);
}

inline void vec_add(float& p, const float fx, float& fw) { p += fw * fx; }

inline float inline_predict(vw& all, example& ec)
{ float temp = ec.l.simple.initial;
  foreach_feature<float, vec_add>(all, ec, temp);
  return temp;
}

inline float sign(float w) { if (w < 0.) return -1.; else  return 1.; }

inline float trunc_weight(const float w, const float gravity)
{
	return (gravity < fabsf(w)) ? w - sign(w) * gravity : 0.f;
}

}
