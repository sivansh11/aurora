#include "common.slang"

[vk::push_constant] push_constant_t pc;

[shader("vertex")]
vertex_shader_output_t vertex_main(uint32_t index: SV_VertexID) {
  vertex_shader_output_t o;

  vertex_t vertex = pc.vertices[pc.indices[index]];
  o.sv_position = float4(vertex.position, 1) * pc.camera.view * pc.camera.projection;

  o.per_fragment.uv = vertex.uv;

  return o;
}
