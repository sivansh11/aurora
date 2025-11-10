#include "renderer.hpp"

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "horizon/core/components.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"
#include "model/model.hpp"

diffuse_renderer_t::diffuse_renderer_t(core::ref<core::window_t> window,   //
                                       core::ref<gfx::context_t> context,  //
                                       core::ref<gfx::base_t>    base,     //
                                       VkFormat                  vk_format)
    : window(window), context(context), base(base) {
  gfx::config_pipeline_layout_t cpl{};
  cpl.add_descriptor_set_layout(base->_bindless_descriptor_set_layout);
  cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
  pl = context->create_pipeline_layout(cpl);

  v = gfx::helper::create_slang_shader(*context, "assets/shaders/diffuse.slang",
                                       gfx::shader_type_t::e_vertex);
  f = gfx::helper::create_slang_shader(*context, "assets/shaders/diffuse.slang",
                                       gfx::shader_type_t::e_fragment);
  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_color_attachment(vk_format, gfx::default_color_blend_attachment());
  VkPipelineDepthStencilStateCreateInfo vk_pipeline_depth_state{};
  vk_pipeline_depth_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  vk_pipeline_depth_state.depthTestEnable   = VK_TRUE;
  vk_pipeline_depth_state.depthWriteEnable  = VK_TRUE;
  vk_pipeline_depth_state.depthCompareOp    = VK_COMPARE_OP_LESS;
  vk_pipeline_depth_state.stencilTestEnable = VK_FALSE;
  cp.set_depth_attachment(VK_FORMAT_D32_SFLOAT, vk_pipeline_depth_state);
  cp.add_shader(v);
  cp.add_shader(f);
  p = context->create_graphics_pipeline(cp);
}

diffuse_renderer_t::~diffuse_renderer_t() {}

void diffuse_renderer_t::render(gfx::handle_commandbuffer_t    cbuf,
                                ecs::scene_t<>                &scene,
                                gfx::handle_buffer_t           camera,
                                gfx::handle_bindless_sampler_t bsampler,
                                VkViewport vk_viewport, VkRect2D vk_scissor) {
  context->cmd_bind_pipeline(cbuf, p);
  context->cmd_bind_descriptor_sets(cbuf, p, 0,
                                    {base->_bindless_descriptor_set});
  context->cmd_set_viewport_and_scissor(cbuf, vk_viewport, vk_scissor);
  scene.for_all<model_t>([&](ecs::entity_id_t id, const model_t &model) {
    push_constant_t pc;
    pc.bsampler = bsampler;
    pc.camera =
        gfx::to<core::camera_t *>(context->get_buffer_device_address(camera));
    for (const auto mesh : model.meshes) {
      push_constant_mesh_t push_constant_mesh{};
      push_constant_mesh.vertices = gfx::to<model::vertex_t *>(
          context->get_buffer_device_address(mesh.vertex_buffer));
      push_constant_mesh.indices = gfx::to<uint32_t *>(
          context->get_buffer_device_address(mesh.index_buffer));
      push_constant_mesh.transform = gfx::to<math::mat4 *>(
          context->get_buffer_device_address(mesh.transform));
      push_constant_mesh.bdiffuse = mesh.bdiffuse;
      pc.push_constant_mesh       = push_constant_mesh;
      context->cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                  sizeof(push_constant_t), &pc);
      context->cmd_draw(cbuf, mesh.index_count, 1, 0, 0);
    }
  });
}

renderer_t::renderer_t(core::ref<core::window_t> window,   //
                       core::ref<gfx::context_t> context,  //
                       core::ref<gfx::base_t>    base,     //
                       const int                 argc,     //
                       const char              **argv)
    : window(window), context(context), base(base), argc(argc), argv(argv) {
  sampler  = context->create_sampler({});
  bsampler = base->new_bindless_sampler();
  base->set_bindless_sampler(bsampler, sampler);

  gfx::config_descriptor_set_layout_t cdsl{};
  cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          VK_SHADER_STAGE_FRAGMENT_BIT);
  cdsl.use_bindless = false;
  imgui_dsl         = context->create_descriptor_set_layout(cdsl);

  imgui_ds = context->allocate_descriptor_set(
      {.handle_descriptor_set_layout = imgui_dsl});

  white = gfx::helper::load_image_from_path_instant(
      *context, base->_command_pool, "assets/images/White_Pixel_1x1.jpg",
      VK_FORMAT_R8G8B8A8_SRGB);
  white_view = context->create_image_view(
      {.handle_image = white,
       .debug_name   = "assets/images/White_Pixel_1x1.jpg"});
  bwhite = base->new_bindless_image();
  base->set_bindless_image(bwhite, white_view,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  {
    gfx::config_buffer_t cb{};
    cb.vk_size               = sizeof(core::camera_t);
    cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    cb.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    camera_buffer =
        base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);
  }

  diffuse_renderer = core::make_ref<diffuse_renderer_t>(
      window, context, base, VK_FORMAT_R32G32B32A32_SFLOAT);
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
    ci.vk_mips   = 1;
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

    ci.vk_format = VK_FORMAT_D32_SFLOAT;
    ci.vk_usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.debug_name    = "depth";
    depth            = context->create_image(ci);
    civ.handle_image = depth;
    civ.debug_name   = "depth view";
    depth_view       = context->create_image_view(civ);

    context->update_descriptor_set(imgui_ds)
        .push_image_write(
            0,
            gfx::image_descriptor_info_t{
                .handle_sampler    = sampler,
                .handle_image_view = image_view,
                .vk_image_layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
        .commit();
  }
}

std::vector<gfx::pass_t> renderer_t::get_passes(ecs::scene_t<>       &scene,
                                                const core::camera_t &camera) {
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
      {
        cb.vk_size = sizeof(raw_mesh.vertices[0]) * raw_mesh.vertices.size();
        cb.vma_allocation_create_flags =
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        cb.debug_name =
            std::to_string(id) + ": " + std::to_string(i) + " vertex buffer";
        mesh.vertex_buffer = gfx::helper::create_buffer_staged(
            *context, base->_command_pool, cb, raw_mesh.vertices.data(),
            cb.vk_size);
      }
      {
        cb.vk_size = sizeof(raw_mesh.indices[0]) * raw_mesh.indices.size();
        cb.vma_allocation_create_flags =
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        cb.debug_name =
            std::to_string(id) + ": " + std::to_string(i) + " index buffer";
        mesh.index_buffer = gfx::helper::create_buffer_staged(
            *context, base->_command_pool, cb, raw_mesh.indices.data(),
            cb.vk_size);
      }

      {
        cb.vk_size = sizeof(math::mat4);
        cb.vma_allocation_create_flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        mesh.transform = context->create_buffer(cb);
        *reinterpret_cast<math::mat4 *>(context->map_buffer(mesh.transform)) =
            scene.get<core::transform_t>(id).mat4();
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

  std::vector<gfx::pass_t> passes;

  VkRect2D vk_rect_2d{};
  vk_rect_2d.extent.width  = width;
  vk_rect_2d.extent.height = height;
  vk_rect_2d.offset        = {};

  auto [viewport, scissor] =
      gfx::helper::fill_viewport_and_scissor_structs(width, height);

  std::memcpy(context->map_buffer(base->buffer(camera_buffer)), &camera,
              sizeof(core::camera_t));

  passes
      .emplace_back(
          [&, vk_rect_2d, viewport, scissor](gfx::handle_commandbuffer_t cbuf) {
            gfx::rendering_attachment_t rendering{};
            rendering.handle_image_view = image_view;
            rendering.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            rendering.load_op      = VK_ATTACHMENT_LOAD_OP_CLEAR;
            rendering.store_op     = VK_ATTACHMENT_STORE_OP_STORE;
            rendering.clear_value.color = {0, 0, 0, 0};
            gfx::rendering_attachment_t depth{};
            depth.handle_image_view = depth_view;
            depth.image_layout      = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depth.load_op           = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth.store_op          = VK_ATTACHMENT_STORE_OP_STORE;
            depth.clear_value.depthStencil.depth = 1;
            context->cmd_begin_rendering(cbuf, {rendering}, depth, vk_rect_2d);

            diffuse_renderer->render(cbuf, scene, base->buffer(camera_buffer),
                                     bsampler, viewport, scissor);

            context->cmd_end_rendering(cbuf);
          })
      .add_read_image(image, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  return passes;
}
