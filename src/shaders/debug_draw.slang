#include "common.slang"

[vk::push_constant]
push_constant_t pc;

struct vertex_stage_output_t {
  float4 sv_position : SV_Position;
  uint32_t id;
};

[shader("vertex")]
vertex_stage_output_t vertex_main(uint32_t id: SV_VertexID) {
  vertex_stage_output_t o;
  o.id = id;

  uint32_t triangle_index = id / 3;
  uint32_t vertex_index = id % 3;

  bvh_triangle_t bvh_triangle = pc.bvh_triangles[triangle_index];

  o.sv_position = reinterpret<bvh_triangle_aliased_t>(bvh_triangle).vertices[vertex_index];
  o.sv_position.w = 1;

  o.sv_position = 
    mul(mul(mul(o.sv_position, *pc.transform), pc.camera.view), pc.camera.projection);

  return o;
}

struct fragment_t {
  float4 color : COLOR0;
};

float3 random_color_from_id(uint32_t v) {
  return {(((v + 1) * 123) % 255) / 255.f, (((v + 1) * 456) % 255) / 255.f,
          (((v + 1) * 789) % 255) / 255.f};
}

[shader("fragment")]
fragment_t fragment_main(uint32_t id) {
  fragment_t fragment;
  fragment.color = float4(random_color_from_id(id), 1); 
  return fragment;
}
