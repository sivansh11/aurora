#ifndef UTILITIES_HPP
#define UTILITIES_HPP

float safe_inverse(float x) {
  const float epsilon = 0.0001;
  const float abs_x = abs(x);
  const float safe_abs_x = max(abs_x, epsilon);
  const float sign_x = sign(x);
  return sign_x / safe_abs_x;
}

#endif
