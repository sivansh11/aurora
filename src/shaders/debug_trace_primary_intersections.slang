#define DEBUG_HIT
// #define DONT_COMBINE_LEAF_PRIMITIVE_INTERSECTIONS
#include "intersect.slang"

[vk::push_constant]
push_constant_t pc;

[vk::binding(0, 0)]
uniform Texture2D textures[1000];
[vk::binding(1, 0)]
uniform SamplerState samplers[1000];
[vk::binding(2, 0)]
uniform RWTexture2D rwtextures[1000];

float4 turbo_color_map(float x) {
    // Source: https://research.google/blog/turbo-an-improved-rainbow-colormap-for-visualization/
    
    const float4 kRedVec4 = float4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
    const float4 kGreenVec4 = float4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
    const float4 kBlueVec4 = float4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
    const float2 kRedVec2 = float2(-152.94239396, 59.28637943);
    const float2 kGreenVec2 = float2(4.27729857, 2.82956604);
    const float2 kBlueVec2 = float2(-89.90310912, 27.34824973);

    x = clamp(x, 0, 1);
    float4 v4 = float4( 1.0, x, x * x, x * x * x);
    float2 v2 = v4.zw * v4.z;
    return float4(
        dot(v4, kRedVec4)   + dot(v2, kRedVec2),
        dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
        dot(v4, kBlueVec4)  + dot(v2, kBlueVec2),
        1
    );
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex) {
  if (dispatch_thread_id.x >= pc.width || dispatch_thread_id.y >= pc.height) return;
  const float u = float(dispatch_thread_id.x) / float(pc.width - 1);
  const float v = float(dispatch_thread_id.y) / float(pc.height - 1);

  ray_t ray = ray_t::create(float2(u, v), pc.camera->inv_projection, pc.camera->inv_view);

  hit_t hit = intersect_bvh(pc.nodes, pc.bvh_prim_indices, pc.bvh_triangles, ray, group_index);

  if (all(rwtextures[pc.bindless_image][uint2(dispatch_thread_id.x, dispatch_thread_id.y)].xyzw == float4(0)))
    rwtextures[pc.bindless_image][uint2(dispatch_thread_id.x, dispatch_thread_id.y)] = turbo_color_map(float(hit.node_intersections + (hit.triangle_intersections * 1.1f)) / float(pc.normalization_constant));
      // rwtextures[pc.bindless_image][uint2(dispatch_thread_id.x, dispatch_thread_id.y)] = float4(float(hit.node_intersections) / float(pc.nodes_intersections_normalize), 
      //                                                                                           float(hit.triangle_intersections) / float(pc.triangles_intersections_normalize), 
      //                                                                                           0, 
      //                                                                                             1);
}
