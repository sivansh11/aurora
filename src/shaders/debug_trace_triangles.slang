#include "intersect.slang"

[vk::push_constant]
push_constant_t pc;

[vk::binding(0, 0)]
uniform Texture2D textures[1000];
[vk::binding(1, 0)]
uniform SamplerState samplers[1000];
[vk::binding(2, 0)]
uniform RWTexture2D rwtextures[1000];

float3 random_color_from_id(uint32_t v) {
  return {(((v * 123) % 255) + 1) / 255.f, 
          (((v * 456) % 255) + 1) / 255.f,
          (((v * 789) % 255) + 1) / 255.f};
}

float3 muted(float3 color, float factor) {
  float3 muted = color * factor;
  float len = length(muted);
  if (len > 0) 
    muted /= len;
  return muted;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex) {
  if (dispatch_thread_id.x >= pc.width || dispatch_thread_id.y >= pc.height) return;
  const float u = float(dispatch_thread_id.x) / float(pc.width - 1);
  const float v = float(dispatch_thread_id.y) / float(pc.height - 1);

  ray_t ray = ray_t::create(float2(u, v), pc.camera->inv_projection, pc.camera->inv_view);

  triangle_hit_t closest_hit;
  closest_hit._did_intersect = false;
  uint32_t closest = 100000;
  

  for (uint32_t i = 0; i < pc.raw_index_count / 3; i++) {
    triangle_hit_t hit = intersect_triangle(pc.bvh_triangles[i], ray);
    if (hit.did_intersect() && hit.t <= ray.tmax) {
      closest = i;
      ray.tmax = hit.t;
      closest_hit = hit;
    }
  }

  if (closest_hit.did_intersect()) {
    if (all(rwtextures[pc.bindless_image][uint2(dispatch_thread_id.x, dispatch_thread_id.y)].xyzw == float4(0)))
      rwtextures[pc.bindless_image][uint2(dispatch_thread_id.x, dispatch_thread_id.y)] = float4(muted(random_color_from_id(closest), 0.1), 1);
  }
}
