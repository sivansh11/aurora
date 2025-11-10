#include "renderer.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "bvh/bvh.hpp"
#include "horizon/core/components.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"
#include "math/triangle.hpp"
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
                                renderer_data_t               &renderer_data,
                                gfx::handle_buffer_t           camera,
                                gfx::handle_bindless_sampler_t bsampler,
                                VkViewport vk_viewport, VkRect2D vk_scissor) {
  context->cmd_bind_pipeline(cbuf, p);
  context->cmd_bind_descriptor_sets(cbuf, p, 0,
                                    {base->_bindless_descriptor_set});
  context->cmd_set_viewport_and_scissor(cbuf, vk_viewport, vk_scissor);

  for (auto cpu_mesh : renderer_data.cpu_meshes) {
    push_constant_t pc;
    pc.bsampler = bsampler;
    pc.camera =
        gfx::to<core::camera_t *>(context->get_buffer_device_address(camera));
    pc.materials =
        context->get_buffer_device_address(renderer_data.materials_buffer);
    pc.gpu_mesh.vertices =
        context->get_buffer_device_address(cpu_mesh.vertex_buffer);
    pc.gpu_mesh.indices =
        context->get_buffer_device_address(cpu_mesh.index_buffer);
    pc.gpu_mesh.transform =
        context->get_buffer_device_address(cpu_mesh.transform);
    pc.gpu_mesh.material_index = cpu_mesh.material_index;
    context->cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                                sizeof(push_constant_t), &pc);
    context->cmd_draw(cbuf, cpu_mesh.index_count, 1, 0, 0);
  }
}

debug_raytracer_t::debug_raytracer_t(core::ref<core::window_t> window,   //
                                     core::ref<gfx::context_t> context,  //
                                     core::ref<gfx::base_t>    base,     //
                                     VkFormat                  vk_format)
    : window(window), context(context), base(base) {
  gfx::config_pipeline_layout_t cpl{};
  cpl.add_descriptor_set_layout(base->_bindless_descriptor_set_layout);
  cpl.add_push_constant(sizeof(push_constant_t), VK_SHADER_STAGE_ALL);
  pl = context->create_pipeline_layout(cpl);

  c = gfx::helper::create_slang_shader(*context,
                                       "assets/shaders/debug_raytracing.slang",
                                       gfx::shader_type_t::e_compute);
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
  cp.add_shader(c);
  p = context->create_compute_pipeline(cp);
}

debug_raytracer_t::~debug_raytracer_t() {}

void debug_raytracer_t::render(gfx::handle_commandbuffer_t    cbuf,
                               renderer_data_t               &renderer_data,
                               gfx::handle_buffer_t           camera,
                               gfx::handle_bindless_sampler_t bsampler,
                               uint32_t width, uint32_t height,
                               gfx::handle_bindless_storage_image_t bsimage) {
  context->cmd_bind_pipeline(cbuf, p);
  context->cmd_bind_descriptor_sets(cbuf, p, 0,
                                    {base->_bindless_descriptor_set});

  push_constant_t pc;
  pc.camera =
      gfx::to<core::camera_t *>(context->get_buffer_device_address(camera));
  pc.triangles = gfx::to<triangle_t *>(
      context->get_buffer_device_address(renderer_data.triangles_buffer));
  pc.bvh2_nodes = gfx::to<bvh::node_t *>(
      context->get_buffer_device_address(renderer_data.bvh2_nodes));
  pc.bvh2_prim_indices = gfx::to<uint32_t *>(
      context->get_buffer_device_address(renderer_data.bvh2_prim_indices));
  pc.cwbvh_nodes = gfx::to<bvh::cnode_t *>(
      context->get_buffer_device_address(renderer_data.cwbvh_nodes));
  pc.cwbvh_prim_indices = gfx::to<uint32_t *>(
      context->get_buffer_device_address(renderer_data.cwbvh_prim_indices));
  pc.width   = width;
  pc.height  = height;
  pc.bsimage = bsimage;
  context->cmd_push_constants(cbuf, p, VK_SHADER_STAGE_ALL, 0,
                              sizeof(push_constant_t), &pc);
  context->cmd_dispatch(cbuf, math::ceil(width / 8), math::ceil(height / 8), 1);
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

  bsimage = base->new_bindless_storage_image();

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
  debug_raytracer = core::make_ref<debug_raytracer_t>(
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

    base->set_bindless_storage_image(bsimage, image_view);
  }
}

std::vector<gfx::pass_t> renderer_t::get_passes(renderer_data_t &renderer_data,
                                                const core::camera_t &camera) {
  std::vector<gfx::pass_t> passes;

  VkRect2D vk_rect_2d{};
  vk_rect_2d.extent.width  = width;
  vk_rect_2d.extent.height = height;
  vk_rect_2d.offset        = {};

  auto [viewport, scissor] =
      gfx::helper::fill_viewport_and_scissor_structs(width, height);

  std::memcpy(context->map_buffer(base->buffer(camera_buffer)), &camera,
              sizeof(core::camera_t));

  switch (rendering_mode) {
    case rendering_mode_t::e_diffuse_raster:

      passes
          .emplace_back([&, vk_rect_2d, viewport,
                         scissor](gfx::handle_commandbuffer_t cbuf) {
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

            diffuse_renderer->render(cbuf, renderer_data,
                                     base->buffer(camera_buffer), bsampler,
                                     viewport, scissor);

            context->cmd_end_rendering(cbuf);
          })
          .add_write_image(image, 0,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      break;

    case rendering_mode_t::e_debug_raytracing:
      passes
          .emplace_back([&](gfx::handle_commandbuffer_t cbuf) {
            debug_raytracer->render(cbuf, renderer_data,
                                    base->buffer(camera_buffer), bsampler,
                                    width, height, bsimage);
          })
          .add_write_image(image, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_IMAGE_LAYOUT_GENERAL);
      break;
  }

  return passes;
}
