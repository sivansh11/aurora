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
  float3 aabbMin;
  uint32_t leftFirst; // 16 bytes
  float3 aabbMax;
  uint32_t triCount; // 16 bytes, total: 32 bytes
};

struct push_constant_t {
  camera_t *camera;
  float4x4 *transform;
  bvh_triangle_t *bvh_triangles;
  vertex_t *vertices;
  uint32_t *raw_indices;
  node_t *nodes;
};
