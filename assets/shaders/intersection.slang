#ifndef INTERSECTION_SLANG
#define INTERSECTION_SLANG

#include "types.slang"

triangle_hit_t intersect_triangle(const triangle_t triangle, 
                                  const ray_t ray) {
  const float3 e1 = triangle.v0.xyz - triangle.v1.xyz;
  const float3 e2 = triangle.v2.xyz - triangle.v0.xyz;
  const float3 n = cross(e1, e2);

  const float3 c = triangle.v0.xyz - ray.origin;
  const float3 r = cross(ray.direction, c);
  const float inverse_det = 1.f / dot(n, ray.direction); // could nan ?

  float u = dot(r, e2) * inverse_det;
  float v = dot(r, e1) * inverse_det;
  float w = 1.f - u - v;

  if (u >= 0 && v >= 0 && w >= 0) {
    float t = dot(n, c) * inverse_det;
    if (t > ray.tmin && t < ray.tmax) {
      triangle_hit_t hit;
      hit.t = t;
      hit.u = u;
      hit.v = v;
      hit._did_intersect = true;
      return hit;
    }
  }
  triangle_hit_t hit;
  hit._did_intersect = false;
  return hit;
}

aabb_hit_t intersect_aabb(const float3 _min, 
                          const float3 _max, 
                          const ray_t ray) {
  float3 tmin = (_min - ray.origin) * ray.inverse_direction;
  float3 tmax = (_max - ray.origin) * ray.inverse_direction;
  const float3 old_tmin = tmin;
  const float3 old_tmax = tmax;
  tmin = min(old_tmin, old_tmax);
  tmax = max(old_tmin, old_tmax);
  float _tmin = max(tmin[0], max(tmin[1], max(tmin[2], ray.tmin)));
  float _tmax = min(tmax[0], min(tmax[1], min(tmax[2], ray.tmax)));
  aabb_hit_t hit = aabb_hit_t(_tmin, _tmax);
  return hit;
}

static const uint32_t SHARED_STACK_SIZE = 16;
groupshared uint32_t shared_bvh2_stack[8 * 8 * 1][SHARED_STACK_SIZE];

hit_t intersect_bvh(bvh2_node_t* nodes, 
                    uint32_t *indices, 
                    triangle_t *triangles, 
                    ray_t ray, 
                    uint group_index) {
  hit_t hit = hit_t();

  uint32_t stack_top = 0;

  bvh2_node_t root = nodes[0];
  if (!intersect_aabb(root.min, root.max, ray).did_intersect()) return hit;

  if (root.is_leaf()) {
    for (uint32_t i = 0; i < root.prim_count; i++) {
      const uint32_t triangle_index = indices[root.first_index + i];
      const triangle_t triangle = triangles[triangle_index];
      triangle_hit_t triangle_hit = intersect_triangle(triangle, ray);
#ifdef DEBUG_HIT
      hit.triangle_intersections++;
#endif
      if (triangle_hit.did_intersect()) {
        ray.tmax = triangle_hit.t;
        hit.prim_index = triangle_index;
        hit.t = triangle_hit.t;
        hit.u = triangle_hit.u;
        hit.v = triangle_hit.v;
      }
    }
    return hit;
  }

  uint32_t current = root.first_index;

  while (true) {
    bvh2_node_t left = nodes[current + 0];
    bvh2_node_t right = nodes[current + 1];

#ifdef DEBUG_HIT
      hit.node_intersections+=1;
#endif
    aabb_hit_t left_hit = intersect_aabb(left.min, left.max, ray);
    aabb_hit_t right_hit = intersect_aabb(right.min, right.max, ray);

#ifndef DONT_COMBINE_LEAF_PRIMITIVE_INTERSECTIONS
    uint32_t start = 0;
    uint32_t end = 0;
    if (left_hit.did_intersect() && left.is_leaf()) {
      if (right_hit.did_intersect() && right.is_leaf()) {
        start = left.first_index;
        end = right.first_index + right.prim_count;
      } else {
        start = left.first_index;
        end = left.first_index + left.prim_count;
      }
    } else {
      if (right_hit.did_intersect() && right.is_leaf()) {
        start = right.first_index;
        end = right.first_index + right.prim_count;
      }
    }
    for (uint32_t index = start; index < end; index++) {
      uint32_t triangle_index = indices[index];
      const triangle_t triangle = triangles[triangle_index];
      triangle_hit_t triangle_hit = intersect_triangle(triangle, ray);
#ifdef DEBUG_HIT
      hit.triangle_intersections++;
#endif
      if (triangle_hit.did_intersect()) {
        ray.tmax = triangle_hit.t;
        hit.prim_index = triangle_index;
        hit.t = triangle_hit.t;
        hit.u = triangle_hit.u;
        hit.v = triangle_hit.v;
      }
    }
#else
    if (left_hit.did_intersect() && left.is_leaf()) {
      for (uint32_t i = 0; i < left.prim_count; i++) {
      const uint32_t triangle_index = indices[left.first_index + i];
        const triangle_t triangle = triangles[triangle_index];
        triangle_hit_t triangle_hit = intersect_triangle(triangle, ray);
#ifdef DEBUG_HIT
        hit.triangle_intersections++;
#endif
        if (triangle_hit.did_intersect()) {
          ray.tmax = triangle_hit.t;
          hit.prim_index = triangle_index;
          hit.t = triangle_hit.t;
          hit.u = triangle_hit.u;
          hit.v = triangle_hit.v;
        }
      }
    }
    if (right_hit.did_intersect() && right.is_leaf()) {
      for (uint32_t i = 0; i < right.prim_count; i++) {
      const uint32_t triangle_index = indices[right.first_index + i];
        const triangle_t triangle = triangles[triangle_index];
        triangle_hit_t triangle_hit = intersect_triangle(triangle, ray);
#ifdef DEBUG_HIT
        hit.triangle_intersections++;
#endif
        if (triangle_hit.did_intersect()) {
          ray.tmax = triangle_hit.t;
          hit.prim_index = triangle_index;
          hit.t = triangle_hit.t;
          hit.u = triangle_hit.u;
          hit.v = triangle_hit.v;
        }
      }
    }
#endif

    if (left_hit.did_intersect() && !left.is_leaf()) {
      if (right_hit.did_intersect() && !right.is_leaf()) {
        if (stack_top >= SHARED_STACK_SIZE) return hit;
        if (left_hit.tmin <= right_hit.tmin) {
          current = left.first_index;
          shared_bvh2_stack[group_index][stack_top++] = right.first_index;
        } else {
          current = right.first_index;
          shared_bvh2_stack[group_index][stack_top++] = left.first_index;
        }
      } else {
        current = left.first_index;
      }
    } else {
      if (right_hit.did_intersect() && !right.is_leaf()) {
        current = right.first_index;
      } else { 
        if (stack_top == 0) return hit;
        current = shared_bvh2_stack[group_index][--stack_top];
      }
    }
  }
  return hit;
}

#endif
