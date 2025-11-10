#include "renderer.hpp"

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <string>

#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "model/model.hpp"

renderer_t::renderer_t(core::ref<core::window_t> window,   //
                       core::ref<gfx::context_t> context,  //
                       core::ref<gfx::base_t>    base,     //
                       const int                 argc,     //
                       const char              **argv)
    : window(window), context(context), base(base), argc(argc), argv(argv) {
  sampler  = context->create_sampler({});
  bsampler = base->new_bindless_sampler();
  base->set_bindless_sampler(bsampler, sampler);

  white = gfx::helper::load_image_from_path_instant(
      *context, base->_command_pool, "assets/images/White_Pixel_1x1.jpg",
      VK_FORMAT_R8G8B8A8_SRGB);
  white_view = context->create_image_view(
      {.handle_image = white,
       .debug_name   = "assets/images/White_Pixel_1x1.jpg"});
  bwhite = base->new_bindless_image();
  base->set_bindless_image(bwhite, white_view,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

renderer_t::~renderer_t() {
  context->destroy_image_view(white_view);
  context->destroy_image(white);
  context->destroy_sampler(sampler);
}

void renderer_t::recreate_sized_resources(uint32_t width, uint32_t height) {
  if (this->width != width || this->height != height) {
    context->wait_idle();

    this->width  = width;
    this->height = height;

    // destroy sized resources
    if (image != core::null_handle) {
      context->destroy_image(image);
    }
    if (image_view != core::null_handle) {
      context->destroy_image_view(image_view);
    }

    // create sized resources
    gfx::config_image_t ci{};
    ci.vk_width  = width;
    ci.vk_height = height;
    ci.vk_depth  = 1;
    ci.vk_type   = VK_IMAGE_TYPE_2D;
    ci.vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ci.vk_usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |  //
                  VK_IMAGE_USAGE_SAMPLED_BIT |            //
                  VK_IMAGE_USAGE_STORAGE_BIT;
    ci.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    ci.debug_name                  = "image";
    image                          = context->create_image(ci);

    gfx::config_image_view_t civ{.handle_image = image,
                                 .debug_name   = "image view"};
    image_view = context->create_image_view(civ);
  }
}

std::vector<gfx::pass_t> renderer_t::get_passes(ecs::scene_t<> &scene) {
  scene.for_all<model::raw_model_t>([&scene, this](
                                        ecs::entity_id_t          id,
                                        const model::raw_model_t &raw_model) {
    if (scene.has<model_t>(id)) return;
    model_t &model = scene.construct<model_t>(id);

    for (uint32_t i = 0; i < raw_model.meshes.size(); i++) {
      const auto raw_mesh = raw_model.meshes[i];
      mesh_t    &mesh     = model.meshes.emplace_back();

      mesh.vertex_count = raw_mesh.vertices.size();
      mesh.index_count  = raw_mesh.indices.size();

      gfx::config_buffer_t cb{};
      cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      {
        cb.vk_size = sizeof(raw_mesh.vertices[0]) * raw_mesh.vertices.size();
        cb.debug_name =
            std::to_string(id) + ": " + std::to_string(i) + " vertex buffer";
        mesh.vertex_buffer = gfx::helper::create_buffer_staged(
            *context, base->_command_pool, cb, raw_mesh.vertices.data(),
            cb.vk_size);
      }
      {
        cb.vk_size = sizeof(raw_mesh.indices[0]) * raw_mesh.indices.size();
        cb.debug_name =
            std::to_string(id) + ": " + std::to_string(i) + " index buffer";
        mesh.index_buffer = gfx::helper::create_buffer_staged(
            *context, base->_command_pool, cb, raw_mesh.indices.data(),
            cb.vk_size);
      }

      auto diffuse_info = std::find_if(
          raw_mesh.material_description.texture_infos.begin(),
          raw_mesh.material_description.texture_infos.end(),
          [](model::texture_info_t info) -> bool {
            return info.texture_type == model::texture_type_t::e_diffuse_map;
          });
      if (diffuse_info != raw_mesh.material_description.texture_infos.end()) {
        mesh.diffuse = gfx::helper::load_image_from_path_instant(
            *context, base->_command_pool, diffuse_info->file_path,
            VK_FORMAT_R8G8B8A8_SRGB);
        mesh.diffuse_view =
            context->create_image_view({.handle_image = mesh.diffuse,
                                        .debug_name = diffuse_info->file_path});

        mesh.bdiffuse = base->new_bindless_image();
        base->set_bindless_image(mesh.bdiffuse, mesh.diffuse_view,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      } else {
        mesh.bdiffuse = bwhite;
      }
      horizon_info("{}: {} mesh", id, i);
    }
  });
  return {};
}
