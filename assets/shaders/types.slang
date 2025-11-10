#ifndef TYPES_SLANG
#define TYPES_SLANG

#include "utilities.slang"

struct camera_t {
  float4x4 view;
  float4x4 inv_view;
  float4x4 projection;
  float4x4 inv_projection;
};

struct vertex_t {
  float3 position;
  float3 normal;
  float2 uv;
  float3 tangent;
  float3 bi_tangent;
};

struct material_t {
  uint32_t bdiffuse;
};

struct gpu_mesh_t {
  vertex_t *vertices;
  uint32_t *indices;
  uint32_t material_index;
  uint32_t padding;
};

struct triangle_t {
  float3 v0, v1, v2;
  uint16_t mesh_index;
  uint16_t material_index;
  float3 normal() {
    float3 e1 = v1 - v0;
    float3 e2 = v2 - v0;
    return normalize(cross(e1, e2));
  }
};

struct triangle_aliased_t {
  float3 vertices[3];
  uint16_t mesh_index;
  uint16_t material_index;
};

struct bvh2_node_t {
  bool is_leaf() { return prim_count > 0; }
  float3 min;
  uint32_t first_index; // 16 bytes
  float3 max;
  uint32_t prim_count; // 16 bytes, total: 32 bytes
};

struct cwbvh_node_t {
  float3 p;
  uint8_t    e[3];
  uint8_t    imask;
  uint32_t   child_base_index;
  uint32_t   primitive_base_index;
  uint8_t    child_meta[8];
  uint8_t    child_min_x[8];
  uint8_t    child_max_x[8];
  uint8_t    child_min_y[8];
  uint8_t    child_max_y[8];
  uint8_t    child_min_z[8];
  uint8_t    child_max_z[8];
};

struct cwbvh_node_aliased_t {
  float4 node_0;
  float4 node_1;
  float4 node_2;
  float4 node_3;
  float4 node_4;
};

struct triangle_hit_t {
  bool did_intersect() { return _did_intersect; }
  float t, u, v;
  bool _did_intersect;
};

struct aabb_hit_t {
  bool did_intersect() {
    return tmin <= tmax;
  }
  float tmin, tmax;
};

static const uint32_t null_index = uint32_t(-1);

struct hit_t {
  bool did_intersect() {
    return prim_index != null_index;
  }
  uint32_t prim_index = null_index;
  float t = 1e30;
  float u, v;
#ifdef DEBUG_HIT
  uint32_t node_intersections = 0;
  uint32_t triangle_intersections = 0;
#endif
};

struct ray_t {
  static ray_t create(float3 origin, float3 direction) {
    ray_t ray;
    ray.origin = origin;
    ray.direction = direction;
    ray.inverse_direction = float3(
      safe_inverse(direction.x),
      safe_inverse(direction.y),
      safe_inverse(direction.z),
    );
    ray.tmax = 1e30;
    ray.tmin = 0.0001;
    return ray;
  }
  static ray_t create(const float2 uv, 
                      const float4x4 inv_projection, 
                      const float4x4 inv_view) {
    const float3 origin = inv_view[3].xyz;
    const float2 xy = uv * 2.f - 1.f; // [-1, 1]
    const float4 clip = float4(xy, -1, 1);
    float4 view = mul(clip, inv_projection);
    view = float4(view.xy, -1, 0);
    const float3 dir = normalize(mul(view, inv_view).xyz);
    return ray_t::create(origin, dir);
  }
  float3 origin, direction, inverse_direction;
  float tmax, tmin;
};

#endif
