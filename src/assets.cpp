#include "assets.hpp"

#include <cassert>
#include <vector>

#include "bvh/bvh.hpp"
#include "horizon/core/components.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/types.hpp"
#include "math/triangle.hpp"
#include "math/utilies.hpp"
#include "model/model.hpp"

void assets_manager_t::load_model_from_path(const std::filesystem::path& path) {
  auto raw_model = model::load_model_from_path(path);
  for (auto& raw_mesh : raw_model.meshes) {
    loaded_meshes.push_back(raw_mesh);
    horizon_info("indices count: {} vertices count: {}",
                 raw_mesh.indices.size(), raw_mesh.vertices.size());
  }
}

renderer_data_t assets_manager_t::prepare(
    core::ref<gfx::base_t> base, core::ref<gfx::context_t> context,
    gfx::handle_bindless_image_t bdefault) {
  std::vector<material_t> materials;
  std::vector<cpu_mesh_t> cpu_meshes;
  std::vector<gpu_mesh_t> gpu_meshes;
  std::vector<triangle_t> triangles;
  for (uint32_t mesh_index = 0; mesh_index < loaded_meshes.size();
       mesh_index++) {
    const auto& raw_mesh     = loaded_meshes[mesh_index];
    cpu_mesh_t& cpu_mesh     = cpu_meshes.emplace_back();
    cpu_mesh.vertex_count    = raw_mesh.vertices.size();
    cpu_mesh.index_count     = raw_mesh.indices.size();
    cpu_mesh.triangle_offset = triangles.size();

    gfx::config_buffer_t cb{};
    cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    {
      cb.vk_size = sizeof(raw_mesh.vertices[0]) * raw_mesh.vertices.size();
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      cpu_mesh.vertex_buffer = gfx::helper::create_buffer_staged(
          *context, base->_command_pool, cb, raw_mesh.vertices.data(),
          cb.vk_size);
    }
    {
      cb.vk_size = sizeof(raw_mesh.indices[0]) * raw_mesh.indices.size();
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      cpu_mesh.index_buffer = gfx::helper::create_buffer_staged(
          *context, base->_command_pool, cb, raw_mesh.indices.data(),
          cb.vk_size);
    }

    {
      cb.vk_size = sizeof(math::mat4);
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
      cpu_mesh.transform = context->create_buffer(cb);
      core::transform_t transform{};
      *reinterpret_cast<math::mat4*>(context->map_buffer(cpu_mesh.transform)) =
          transform.mat4();
    }

    // cpu_mesh.material_index = materials.size();
    material_t& material = materials.emplace_back();

    auto diffuse_info = std::find_if(
        raw_mesh.material_description.texture_infos.begin(),
        raw_mesh.material_description.texture_infos.end(),
        [](model::texture_info_t info) -> bool {
          return info.texture_type == model::texture_type_t::e_diffuse_map;
        });
    if (diffuse_info != raw_mesh.material_description.texture_infos.end()) {
      cpu_mesh.diffuse = gfx::helper::load_image_from_path_instant(
          *context, base->_command_pool, diffuse_info->file_path,
          VK_FORMAT_R8G8B8A8_SRGB);
      cpu_mesh.diffuse_view =
          context->create_image_view({.handle_image = cpu_mesh.diffuse,
                                      .debug_name   = diffuse_info->file_path});

      material.bdiffuse = base->new_bindless_image();
      base->set_bindless_image(material.bdiffuse, cpu_mesh.diffuse_view,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
      material.bdiffuse = bdefault;
    }

    auto raw_triangles = model::create_triangles_from_mesh(raw_mesh);
    for (auto triangle : raw_triangles)
      triangles.emplace_back(triangle, mesh_index);

    gpu_mesh_t& gpu_mesh = gpu_meshes.emplace_back();
    gpu_mesh.vertices    = gfx::to<model::vertex_t*>(
        context->get_buffer_device_address(cpu_mesh.vertex_buffer));
    gpu_mesh.indices = gfx::to<uint32_t*>(
        context->get_buffer_device_address(cpu_mesh.index_buffer));
    gpu_mesh.transform = gfx::to<math::mat4*>(
        context->get_buffer_device_address(cpu_mesh.transform));
    gpu_mesh.vertex_count    = cpu_mesh.vertex_count;
    gpu_mesh.index_count     = cpu_mesh.index_count;
    gpu_mesh.triangle_offset = cpu_mesh.triangle_offset;
  }

  gfx::handle_buffer_t triangles_buffer;
  gfx::handle_buffer_t bvh2_nodes;
  gfx::handle_buffer_t bvh2_prim_indices;
  gfx::handle_buffer_t materials_buffer;
  gfx::handle_buffer_t meshes_buffer;

  gfx::config_buffer_t cb{};
  cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  {
    cb.vk_size                     = sizeof(triangles[0]) * triangles.size();
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    triangles_buffer               = gfx::helper::create_buffer_staged(
        *context, base->_command_pool, cb, triangles.data(), cb.vk_size);
  }

  std::vector<math::triangle_t> tmp_triangles{};
  for (auto triangle : triangles) tmp_triangles.push_back(triangle.triangle);

  auto [aabbs, tri_indices] = bvh::presplit(tmp_triangles, 0.3);
  // auto aabbs = math::aabbs_from_triangles(tmp_triangles);

  bvh::bvh_t bvh2 = bvh::build_bvh_sweep_sah(aabbs);
  bvh::presplit_remove_indirection(bvh2, tri_indices);
  bvh::presplit_remove_duplicates(bvh2);

  {
    cb.vk_size                     = sizeof(bvh2.nodes[0]) * bvh2.nodes.size();
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    bvh2_nodes                     = gfx::helper::create_buffer_staged(
        *context, base->_command_pool, cb, bvh2.nodes.data(), cb.vk_size);
  }
  {
    cb.vk_size = sizeof(bvh2.prim_indices[0]) * bvh2.prim_indices.size();
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    bvh2_prim_indices =
        gfx::helper::create_buffer_staged(*context, base->_command_pool, cb,
                                          bvh2.prim_indices.data(), cb.vk_size);
  }

  {
    cb.vk_size                     = sizeof(materials[0]) * materials.size();
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    materials_buffer               = gfx::helper::create_buffer_staged(
        *context, base->_command_pool, cb, materials.data(), cb.vk_size);
  }
  {
    cb.vk_size                     = sizeof(gpu_meshes[0]) * gpu_meshes.size();
    cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    meshes_buffer                  = gfx::helper::create_buffer_staged(
        *context, base->_command_pool, cb, gpu_meshes.data(), cb.vk_size);
  }
  return {
      triangles_buffer,
      bvh2_nodes,
      bvh2_prim_indices,
      materials_buffer,
      meshes_buffer,
      cpu_meshes,
      (uint32_t)materials.size(),
      (uint32_t)gpu_meshes.size(),
      (uint32_t)triangles.size(),
  };
}
