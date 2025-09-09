struct triangle_t {
  float4 v0, v1, v2;
};

struct triangle_aliased_t {
  float4 vertices[3];
};

struct push_constant_t {
  triangle_t *triangles;
  float4x4 *view;
  float4x4 *projection;
  float4x4 *inv_view;
  float4x4 *inv_projection;
  float4x4 *model;
};
[vk::push_constant]
push_constant_t pc;

struct vertex_stage_output_t {
  float4 sv_position : SV_Position;
};

[shader("vertex")]
vertex_stage_output_t vertex_main(uint32_t id: SV_VertexID) {
  vertex_stage_output_t o;
  uint32_t triangle_index = id / 3;
  uint32_t vertex_index = id % 3;

  triangle_t triangle = pc.triangles[triangle_index];

  o.sv_position =
      reinterpret<triangle_aliased_t>(triangle).vertices[vertex_index];
  // o.pos = o.sv_position.xyz;
  o.sv_position.w = 1;
  o.sv_position =
      mul(mul(mul(o.sv_position, *pc.model), *pc.view), *pc.projection);

  return o;
}

struct fragment_t {
  float4 color : COLOR0;
};

[shader("fragment")]
fragment_t fragment_main() {
  fragment_t fragment;
  fragment.color = float4(1, 1, 1, 1);
  return fragment;
}
