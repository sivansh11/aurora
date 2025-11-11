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

inline uint32_t extract_byte(uint32_t x, uint32_t i) {
  return (x >> (i * 8)) & 0xff;
}

inline uint32_t sign_extend_s8x4(uint32_t x) {
  return ((x >> 7) & 0x01010101) * 0xff;
}

inline uint32_t cwbvh_node_intersect(const ray_t ray, 
                                     uint32_t oct_inv4, 
                                     float max_distance, 
                                     const cwbvh_node_aliased_t node) {
  float3 p = float3(node.node_0.xyz);

  uint32_t e_imask = asuint(node.node_0.w);
  uint32_t e_x     = extract_byte(e_imask, 0);
  uint32_t e_y     = extract_byte(e_imask, 1);
  uint32_t e_z     = extract_byte(e_imask, 2);

  float3 adjusted_ray_direction_inv =
      float3(asfloat(uint32_t(e_x) << 23) / ray.direction.x,
                 asfloat(uint32_t(e_y) << 23) / ray.direction.y,
                 asfloat(uint32_t(e_z) << 23) / ray.direction.z);

  float3 adjusted_ray_origin = (p - ray.origin) / ray.direction;

  uint32_t hit_mask = 0;

  [unroll]
  for (int i = 0; i < 2; i++) {
    uint32_t meta4 =
        asuint(i == 0 ? node.node_1.z : node.node_1.w);

    uint32_t is_inner4   = (meta4 & (meta4 << 1)) & 0x10101010;
    uint32_t inner_mask4 = sign_extend_s8x4(is_inner4 << 3);
    uint32_t bit_index4  = (meta4 ^ (oct_inv4 & inner_mask4)) & 0x1f1f1f1f;
    uint32_t child_bits4 = (meta4 >> 5) & 0x07070707;

    // Select near and far planes based on ray octant
    uint32_t q_lo_x =
        asuint(i == 0 ? node.node_2.x : node.node_2.y);
    uint32_t q_hi_x =
        asuint(i == 0 ? node.node_2.z : node.node_2.w);

    uint32_t q_lo_y =
        asuint(i == 0 ? node.node_3.x : node.node_3.y);
    uint32_t q_hi_y =
        asuint(i == 0 ? node.node_3.z : node.node_3.w);

    uint32_t q_lo_z =
        asuint(i == 0 ? node.node_4.x : node.node_4.y);
    uint32_t q_hi_z =
        asuint(i == 0 ? node.node_4.z : node.node_4.w);

    uint32_t x_min = ray.direction.x < 0.0 ? q_hi_x : q_lo_x;
    uint32_t x_max = ray.direction.x < 0.0 ? q_lo_x : q_hi_x;

    uint32_t y_min = ray.direction.y < 0.0 ? q_hi_y : q_lo_y;
    uint32_t y_max = ray.direction.y < 0.0 ? q_lo_y : q_hi_y;

    uint32_t z_min = ray.direction.z < 0.0 ? q_hi_z : q_lo_z;
    uint32_t z_max = ray.direction.z < 0.0 ? q_lo_z : q_hi_z;

    [unroll]
    for (int j = 0; j < 4; j++) {
      // Extract j-th byte
      float3 tmin3 = float3(float(extract_byte(x_min, j)),
                                    float(extract_byte(y_min, j)),
                                    float(extract_byte(z_min, j)));
      float3 tmax3 = float3(float(extract_byte(x_max, j)),
                                    float(extract_byte(y_max, j)),
                                    float(extract_byte(z_max, j)));

      // Account for grid origin and scale
      tmin3 = tmin3 * adjusted_ray_direction_inv + adjusted_ray_origin;
      tmax3 = tmax3 * adjusted_ray_direction_inv + adjusted_ray_origin;

      float tmin = max(max(tmin3.x, tmin3.y), max(tmin3.z, ray.tmin));
      float tmax = min(min(tmax3.x, tmax3.y), min(tmax3.z, max_distance));

      bool intersected = tmin < tmax + 1e-6f;
      if (intersected) {
        uint32_t child_bits = extract_byte(child_bits4, j);
        uint32_t bit_index  = extract_byte(bit_index4, j);
        hit_mask |= child_bits << bit_index;
      }
    }
  }

  return hit_mask;
}

groupshared uint2 shared_cwbvh_stack[8 * 8 * 1][SHARED_STACK_SIZE];

uint32_t ray_get_octant_inv4(const float3 direction) {
  uint32_t octant = 0;
  octant |= (direction.x < 0) ? 1 : 0;
  octant |= (direction.y < 0) ? 2 : 0;
  octant |= (direction.z < 0) ? 4 : 0;
  return octant * 0x01010101;  // Replicate to all 4 bytes
}

hit_t intersect_cwbvh(cwbvh_node_aliased_t *nodes, 
                      uint32_t *indices, 
                      triangle_t *triangles, 
                      ray_t ray, 
                      uint group_index) {
  hit_t hit = hit_t();

  // const uint32_t LOCAL_STACK_SIZE = 64;
  // uint2 stack[LOCAL_STACK_SIZE];
  uint32_t       stack_size = 0;

  uint32_t    oct_inv4      = ray_get_octant_inv4(ray.direction);
  uint2 current_group = uint2(0, 0x80000000);

  while (stack_size > 0 || current_group.y != 0) {
    uint2 triangle_group;
    if ((current_group.y & 0xff000000) != 0) {
      uint32_t hits_imask         = current_group.y;
      uint32_t child_index_offset = firstbithigh(hits_imask);
      uint32_t child_index_base   = current_group.x;

      // Remove n from current_group;
      current_group.y &= ~(1 << child_index_offset);

      // If the node group is not yet empty, push it on the stack
      if ((current_group.y & 0xff000000) != 0) {
        // stack_push(stack, stack_size, current_group);
        shared_cwbvh_stack[group_index][stack_size++] = current_group;
      }

      uint32_t slot_index = (child_index_offset - 24) ^ (oct_inv4 & 0xff);
      uint32_t relative_index =
          countbits(hits_imask & ~(0xffffffff << slot_index));

      uint32_t            child_node_index = child_index_base + relative_index;
      const cwbvh_node_aliased_t cnode            = nodes[child_node_index];
#ifdef DEBUG_HIT
      hit.node_intersections++;
#endif
      uint32_t hitmask = cwbvh_node_intersect(ray, oct_inv4, hit.t, cnode);

      uint32_t imask = extract_byte(asuint(cnode.node_0.w), 3);

      current_group.x =
          asuint(cnode.node_1.x);  // Child    base offset
      triangle_group.x =
          asuint(cnode.node_1.y);  // Triangle base offset

      current_group.y  = (hitmask & 0xff000000) | imask;
      triangle_group.y = (hitmask & 0x00ffffff);
    } else {
      triangle_group = current_group;
      current_group  = uint2(0);
    }

    while (triangle_group.y != 0) {
      uint32_t triangle_index = firstbithigh(triangle_group.y);
      triangle_group.y &= ~(1 << triangle_index);

      uint32_t tri_idx = uint32_t(triangle_group.x + triangle_index);
      float    t, u, v;

      const uint32_t         index        = indices[tri_idx];
      const triangle_t triangle     = triangles[index];
      triangle_hit_t         triangle_hit = intersect_triangle(triangle, ray);
#ifdef DEBUG_HIT
      hit.triangle_intersections++;
#endif
      if (triangle_hit.did_intersect()) {
        ray.tmax       = triangle_hit.t;
        hit.prim_index = index;
        hit.t          = triangle_hit.t;
        hit.u          = triangle_hit.u;
        hit.v          = triangle_hit.v;
      }
    }

    if ((current_group.y & 0xff000000) == 0) {
      if (stack_size == 0) break;
      // current_group = stack_pop(stack, stack_size);
      current_group = shared_cwbvh_stack[group_index][--stack_size];
    }
  }
  return hit;

}

#endif
