#include "intersection.slang"
#include "types.slang"

struct push_constant_t {
  camera_t              *camera;
  
  gpu_mesh_t            *meshes;
  material_t            *materials;

  triangle_t            *triangles;

  bvh2_node_t           *bvh2_nodes;
  uint32_t              *bvh2_prim_indices;

  uint32_t              width;
  uint32_t              height;
  
  uint32_t              bsimage;
  uint32_t              bsampler;

  uint32_t              triangles_count;
  uint32_t              padding;

  uint32_t              materials_count;
  uint32_t              meshes_count;
};


[vk::push_constant] push_constant_t pc;

[vk::binding(0, 0)]
uniform Texture2D textures[1000];
[vk::binding(1, 0)]
uniform SamplerState samplers[1000];
[vk::binding(2, 0)]
uniform RWTexture2D rwtextures[1000];

vertex_t barry(float u, float v, float w, triangle_t triangle, gpu_mesh_t mesh, uint32_t prim_index) {
  vertex_t v0, v1, v2, vertex;
  v0 = mesh.vertices[mesh.indices[(prim_index - mesh.triangle_offset) * 3 + 0]];
  v1 = mesh.vertices[mesh.indices[(prim_index - mesh.triangle_offset) * 3 + 1]];
  v2 = mesh.vertices[mesh.indices[(prim_index - mesh.triangle_offset) * 3 + 2]];

  vertex.position = u * v0.position + v * v1.position + w * v2.position;          
  vertex.normal = u * v0.normal + v * v1.normal + w * v2.normal;
  vertex.uv = u * v0.uv + v * v1.uv + w * v2.uv;                                  
  vertex.tangent = u * v0.tangent + v * v1.tangent + w * v2.tangent;
  vertex.bi_tangent = u * v0.bi_tangent + v * v1.bi_tangent + w * v2.bi_tangent;  
  return vertex;                                                                  
}

float3 random_color_from_id(uint32_t v) {
  return {(((v * 123) % 255) + 1) / 255.f, 
          (((v * 456) % 255) + 1) / 255.f,
          (((v * 789) % 255) + 1) / 255.f};
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID, 
                  uint group_index : SV_GroupIndex) {
  if (dispatch_thread_id.x >= pc.width ||
      dispatch_thread_id.y >= pc.height)
    return;

  const float u = float(dispatch_thread_id.x) / float(pc.width - 1);
  const float v = float(dispatch_thread_id.y) / float(pc.height - 1);

  ray_t ray = ray_t::create(float2(u, v),
                            pc.camera->inv_projection,
                            pc.camera->inv_view);

  hit_t hit = intersect_bvh(pc.bvh2_nodes, 
                            pc.bvh2_prim_indices, 
                            pc.triangles, 
                            ray, 
                            group_index);

  if (hit.did_intersect()) {
    triangle_t triangle = pc.triangles[hit.prim_index];
    gpu_mesh_t mesh = pc.meshes[triangle.mesh_index];
    vertex_t v = barry(
                       1.f - hit.u - hit.v, 
                       hit.u, 
                       hit.v, 
                       triangle, 
                       mesh, 
                       hit.prim_index);
    rwtextures[pc.bsimage][uint2(dispatch_thread_id.x, dispatch_thread_id.y)]
      = textures[NonUniformResourceIndex(pc.materials[triangle.mesh_index].bdiffuse)]
        .Sample(samplers[pc.bsampler], v.uv);
  } else {
    rwtextures[pc.bsimage][uint2(dispatch_thread_id.x, dispatch_thread_id.y)]
      = float4(0,0,0,0);
  }
}
