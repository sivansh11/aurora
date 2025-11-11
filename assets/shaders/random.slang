#ifndef RANDOM_SLANG
#define RANDOM_SLANG

uint pcg_hash(uint seed) {
  uint state = seed * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// advance pcg state
uint _rand_pcg(inout uint rng_state) {
  uint state = rng_state;
  rng_state = rng_state * 747796405u + 2891336453u; 
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

// advances prng state and returns a float
float random_float(inout uint state) {
  uint x = _rand_pcg(state);
  state = x;
  return float(x) * asfloat(0x2f800004u);
}

float3 random_float3_in_unit_sphere(inout uint state) {
  float theta = 2 * 3.1415 * random_float(state);
  float rho = sqrt(-2 * log(random_float(state)));
  return rho * cos(theta);
}

float3 random_float3_unit_sphere(inout uint state) {
  return random_float3_in_unit_sphere(state);
}

#endif
