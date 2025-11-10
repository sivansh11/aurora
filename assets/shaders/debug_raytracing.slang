#define USE_CWBVH
#define DEBUG_HIT // to enable intersection stats in hit
#include "intersection.slang"
#include "types.slang"

struct push_constant_t {
  camera_t              *camera;
  
  triangle_t            *triangles;

  bvh2_node_t           *bvh2_nodes;
  uint32_t              *bvh2_prim_indices;

  cwbvh_node_aliased_t  *cwbvh_nodes;
  uint32_t              *cwbvh_prim_indices;

  uint32_t              width;
  uint32_t              height;
  
  uint32_t              bsimage;
  uint32_t              padding;
};

[vk::push_constant] push_constant_t pc;

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

  float value = hit.node_intersections +
            (hit.triangle_intersections * 1.1f);

  rwtextures[pc.bsimage][uint2(dispatch_thread_id.x, dispatch_thread_id.y)]
    = turbo_color_map(value / 150.f);
}
