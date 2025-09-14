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

  uint32_t node_index = id / 24;
  uint32_t vertex_index = id % 24;

  node_t node = pc.nodes[node_index];

  float3 final_p = 0;

  switch (vertex_index) {
    case 0:
      final_p = {node.min.x, node.min.y, node.min.z};
    break;
    case 1:
      final_p = {node.max.x, node.min.y, node.min.z};
    break;
    case 2:
      final_p = {node.max.x, node.min.y, node.min.z};
    break;
    case 3:
      final_p = {node.max.x, node.max.y, node.min.z};
    break;
    case 4:
      final_p = {node.max.x, node.max.y, node.min.z};
    break;
    case 5:
      final_p = {node.min.x, node.max.y, node.min.z};
    break;
    case 6:
      final_p = {node.min.x, node.max.y, node.min.z};
    break;
    case 7:
      final_p = {node.min.x, node.min.y, node.min.z};
    break;
    case 8:
      final_p = {node.min.x, node.min.y, node.max.z};
    break;
    case 9:
      final_p = {node.max.x, node.min.y, node.max.z};
    break;
    case 10:
      final_p = {node.max.x, node.min.y, node.max.z};
    break;
    case 11:
      final_p = {node.max.x, node.max.y, node.max.z};
    break;
    case 12:
      final_p = {node.max.x, node.max.y, node.max.z};
    break;
    case 13:
      final_p = {node.min.x, node.max.y, node.max.z};;
    break;
    case 14:
      final_p = {node.min.x, node.max.y, node.max.z};;
    break;
    case 15:
      final_p = {node.min.x, node.min.y, node.max.z};
    break;
    case 16:
      final_p = {node.min.x, node.min.y, node.min.z};
    break;
    case 17:
      final_p = {node.min.x, node.min.y, node.max.z};
    break;
    case 18:
      final_p = {node.max.x, node.min.y, node.min.z};
    break;
    case 19:
      final_p = {node.max.x, node.min.y, node.max.z};
    break;
    case 20:
      final_p = {node.max.x, node.max.y, node.min.z};
    break;
    case 21:
      final_p = {node.max.x, node.max.y, node.max.z};
    break;
    case 22:
      final_p = {node.min.x, node.max.y, node.min.z};
    break;
    case 23:
      final_p = {node.min.x, node.max.y, node.max.z};;
    break;
  }

  o.sv_position = float4(final_p, 1);

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
  fragment.color = float4(1, 1, 1, 1); 
  return fragment;
}
