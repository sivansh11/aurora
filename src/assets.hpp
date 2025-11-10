#ifndef ASSETS_HPP
#define ASSETS_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <vector>

#include "horizon/core/core.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/types.hpp"
#include "math/triangle.hpp"
#include "model/model.hpp"

struct material_t {
  gfx::handle_bindless_image_t bdiffuse;
};

struct cpu_mesh_t {
  gfx::handle_buffer_t vertex_buffer;
  gfx::handle_buffer_t index_buffer;

  uint32_t vertex_count;
  uint32_t index_count;

  gfx::handle_buffer_t transform;

  gfx::handle_image_t      diffuse;
  gfx::handle_image_view_t diffuse_view;

  uint32_t material_index;
};

struct gpu_mesh_t {
  VkDeviceAddress vertices;
  VkDeviceAddress indices;
  VkDeviceAddress transform;
  uint32_t        material_index;
  uint32_t        padding;
};

// // TODO: experiment with more efficient triangle data formats for
struct triangle_t {
  math::triangle_t triangle;
  uint16_t         mesh_index;
  uint16_t         material_index;
};
static_assert(sizeof(triangle_t) == 40, "sizeof(triangle_t) should be 40");

struct renderer_data_t {
  gfx::handle_buffer_t triangles_buffer;
  gfx::handle_buffer_t bvh2_nodes;
  gfx::handle_buffer_t bvh2_prim_indices;
  gfx::handle_buffer_t cwbvh_nodes;
  gfx::handle_buffer_t cwbvh_prim_indices;
  gfx::handle_buffer_t materials_buffer;
  gfx::handle_buffer_t meshes_buffer;

  std::vector<cpu_mesh_t> cpu_meshes;
};

struct assets_manager_t {
  void            load_model_from_path(const std::filesystem::path& model_path);
  renderer_data_t prepare(core::ref<gfx::base_t>       base,
                          core::ref<gfx::context_t>    context,
                          gfx::handle_bindless_image_t bdefault);
  std::vector<model::raw_mesh_t> loaded_meshes;
};

#endif
