struct camera_t {
  float4x4 view;
  float4x4 inv_view;
  float4x4 projection;
  float4x4 inv_projection;
};

struct bvh_triangle_t {
  float4 v0, v1, v2;
};

struct bvh_triangle_aliased_t {
  float4 vertices[3];
};

struct vertex_t {
  float3 position;
  float3 normal;
  float2 uv;
  float3 tangent;
  float3 bi_tangent;
};

struct node_t {
  bool is_leaf() { return prim_count > 0; }
  float3 min;
  uint32_t first_index; // 16 bytes
  float3 max;
  uint32_t prim_count; // 16 bytes, total: 32 bytes
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
};

struct debug_hit_t {
  bool did_intersect() {
    return prim_index != null_index;
  }
  uint32_t prim_index = null_index;
  float t = 1e30;
  float u, v;
  uint32_t node_intersections = 0;
  uint32_t triangle_intersections = 0;
};

float safe_inverse(float x) {
  const float epsilon = 0.0001;
  const float abs_x = abs(x);
  const float safe_abs_x = max(abs_x, epsilon);
  const float sign_x = sign(x);
  return sign_x / safe_abs_x;
}

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
  static ray_t create(const float2 uv, const float4x4 inv_projection, const float4x4 inv_view) {
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

struct push_constant_t {
  camera_t *camera;
  float4x4 *transform;
  bvh_triangle_t *bvh_triangles;
  uint32_t *bvh_prim_indices;
  vertex_t *vertices;
  uint32_t *raw_indices;
  node_t *nodes;

  uint32_t width;
  uint32_t height;
  uint32_t raw_index_count;
  uint32_t bindless_image;
  uint32_t node_count;
  uint32_t nodes_intersections_normalize;
  uint32_t triangles_intersections_normalize;
};
