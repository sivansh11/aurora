#include "types.slang"

struct mesh_t {
  vertex_t *vertices;
  uint32_t *indices;
  float4x4 *transform;
  uint32_t bdiffuse;
  uint32_t padding;
};

struct push_constant_t {
  camera_t *camera;

  mesh_t mesh;

  uint32_t bsampler;
  uint32_t padding;
};

[vk::push_constant] push_constant_t pc;

[vk::binding(0, 0)]
uniform Texture2D textures[1000];
[vk::binding(1, 0)]
uniform SamplerState samplers[1000];

struct vertex_stage_output_t {
  float4 sv_position: SV_Position;
  uint32_t index;
  float2 uv;
};

[shader("vertex")]
vertex_stage_output_t vertex_main(uint32_t id: SV_VertexID) {
  vertex_stage_output_t o;

  const uint32_t vertex_index = pc.mesh.indices[id];
  const vertex_t vertex = pc.mesh.vertices[vertex_index];

  o.index = vertex_index;
  o.uv = vertex.uv;

  o.sv_position.xyz = vertex.position;
  o.sv_position.w = 1;
  
  o.sv_position = 
  mul(mul(mul(o.sv_position, *pc.mesh.transform), pc.camera.view), pc.camera.projection);

  return o;
}

struct fragment_t {
  float4 color : COLOR0;
};

[shader("fragment")]
fragment_t fragment_main(uint32_t index, float2 uv) {
  fragment_t f;
  f.color = textures[pc.mesh.bdiffuse].Sample(samplers[pc.bsampler], uv);
  return f;
}
