#ifndef UTILITIES_SLANG
#define UTILITIES_SLANG

float safe_inverse(float x) {
  const float epsilon = 0.0001;
  const float abs_x = abs(x);
  const float safe_abs_x = max(abs_x, epsilon);
  const float sign_x = sign(x);
  const float sign_x = x >= 0 ? +1 : -1;
  return sign_x / safe_abs_x;
}

#endif
