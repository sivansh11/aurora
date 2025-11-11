// #define USE_CWBVH
#include "intersection.slang"
#include "random.slang"
#include "types.slang"

struct push_constant_t {
  camera_t              *camera;
  
  gpu_mesh_t            *meshes;
  material_t            *materials;

  triangle_t            *triangles;

  bvh2_node_t           *bvh2_nodes;
  uint32_t              *bvh2_prim_indices;

  cwbvh_node_aliased_t  *cwbvh_nodes;
  uint32_t              *cwbvh_prim_indices;

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

float3 background(ray_t ray) {
  float3 unit_direction = normalize(ray.direction);
  float a = 0.5 * (unit_direction.y + 1.0);
  return (1.0 - a) * float3(1, 1, 1) + a * float3(0.3, 0.4, 0.7);
}

float3 material_emitted(const material_t material, hit_t hit) {
  // TODO: better material types
  return float3(0, 0, 0);
}

bool near_zero(float3 v) {
  const float s = 1e-8;
  return (abs(v.x) < s) &&
         (abs(v.y) < s) &&
         (abs(v.z) < s);
}

bool material_scatter(const material_t material, 
                      inout uint seed, 
                      const vertex_t vertex,
                      const ray_t ray, 
                      const hit_t hit, 
                      out float3 attenuation, 
                      out ray_t scattered) {
  float3 n = vertex.normal;
  bool front_face = dot(ray.direction, n) < 0;
  n = front_face ? n : -n;
  
  // TODO: better material types
  // assuming lambertian
  float3 scatter_direction = n + random_float3_unit_sphere(seed);
  
  if (near_zero(scatter_direction))
    scatter_direction = n;
  scattered = ray_t::create(ray.origin + hit.t * ray.direction, scatter_direction);
  attenuation = random_color_from_id(hit.prim_index);
  return true;
}

bool russian_roulette_terminate_ray(inout float3 throughput, inout uint seed) {
  float p = max(throughput.x, max(throughput.y, throughput.z));
  // TODO: make sure random_float is between 0 and 1
  if (random_float(seed) > p) {
    return true;
  }
  throughput *= 1 / p;
  return false;
}

float3 ray_color(ray_t ray, inout uint seed, uint group_index) {
  const uint32_t bounces = 3;

  float3 color = float3(0, 0, 0);
  float3 throughput = float3(1, 1, 1);

  for (uint32_t bounce = 0; bounce < bounces + 1; bounce++) {
#ifdef USE_CWBVH
    hit_t hit = intersect_cwbvh(pc.cwbvh_nodes, 
                                pc.cwbvh_prim_indices, 
                                pc.triangles, 
                                ray, 
                                group_index);
#else
    hit_t hit = intersect_bvh(pc.bvh2_nodes, 
                              pc.bvh2_prim_indices, 
                              pc.triangles, 
                              ray, 
                              group_index);
#endif
    if (!hit.did_intersect()) {
      color += throughput * background(ray);
      break;
    }

    triangle_t triangle = pc.triangles[hit.prim_index];
    material_t material = pc.materials[triangle.mesh_index];
    gpu_mesh_t mesh = pc.meshes[triangle.mesh_index];
    vertex_t v = barry(
                       1.f - hit.u - hit.v, 
                       hit.u, 
                       hit.v, 
                       triangle, 
                       mesh, 
                       hit.prim_index);

    float3 emission = material_emitted(material, hit);
    color += throughput * emission;
    
    float3 attenuation;
    ray_t scattered;

    if (!material_scatter(material, seed, v, ray, hit, attenuation, scattered)) {
      break;
    }

    throughput = throughput * attenuation;
    ray = scattered;

    if (russian_roulette_terminate_ray(throughput, seed)) {
      break;
    }
  }
  return color;
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

  uint seed = uint(dispatch_thread_id.x + pc.width * 
                   (dispatch_thread_id.y + pc.height)); 
  float3 color = ray_color(ray, seed, group_index);
  rwtextures[pc.bsimage][uint2(dispatch_thread_id.x, dispatch_thread_id.y)]
    = float4(color, 1);
}
