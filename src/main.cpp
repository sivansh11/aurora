#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>

#include "horizon/core/components.hpp"
#include "horizon/gfx/types.hpp"
#include "math/triangle.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <GLFW/glfw3.h>
#include <optional>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include "bvh/bvh.hpp"
#include "camera.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "model/model.hpp"

struct bvh_t {
  gfx::handle_buffer_t nodes;
  gfx::handle_buffer_t triangles;
  gfx::handle_buffer_t indices;
};

namespace shader {

struct bvh_t {
  VkDeviceAddress nodes;
  VkDeviceAddress triangles;
  VkDeviceAddress indices;
};

struct push_constant_t {
  VkDeviceAddress triangles;
  VkDeviceAddress view;
  VkDeviceAddress projection;
  VkDeviceAddress inv_view;
  VkDeviceAddress inv_projection;
  VkDeviceAddress model;
};
static_assert(sizeof(push_constant_t) <= 128,
              "sizeof(push_constant_t) should be less than= 128");

}  // namespace shader

bvh_t from(const bvh::bvh_t &bvh, core::ref<gfx::base_t> base) {
  bvh_t converted;

  gfx::config_buffer_t cb{};

  cb.vk_buffer_usage_flags       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  cb.vk_size                     = bvh.nodes.size() * sizeof(bvh.nodes[0]);
  converted.nodes                = gfx::helper::create_buffer_staged(
      *base->_context, base->_command_pool, cb, bvh.nodes.data(),
      bvh.nodes.size() * sizeof(bvh.nodes[0]));

  cb.vk_buffer_usage_flags       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  cb.vk_size          = bvh.triangles.size() * sizeof(bvh.triangles[0]);
  converted.triangles = gfx::helper::create_buffer_staged(
      *base->_context, base->_command_pool, cb, bvh.triangles.data(),
      bvh.triangles.size() * sizeof(bvh.triangles[0]));

  cb.vk_buffer_usage_flags       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  cb.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  cb.vk_size                     = bvh.indices.size() * sizeof(bvh.indices[0]);
  converted.indices              = gfx::helper::create_buffer_staged(
      *base->_context, base->_command_pool, cb, bvh.indices.data(),
      bvh.indices.size() * sizeof(bvh.indices[0]));

  return converted;
}

shader::bvh_t from(bvh_t bvh, core::ref<gfx::base_t> base) {
  shader::bvh_t converted;
  bvh.nodes     = base->_context->get_buffer_device_address(bvh.nodes);
  bvh.triangles = base->_context->get_buffer_device_address(bvh.triangles);
  bvh.indices   = base->_context->get_buffer_device_address(bvh.indices);
  return converted;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: aurora model\n";
    exit(EXIT_FAILURE);
  }

  auto window  = core::make_ref<core::window_t>("aurora", 640, 420);
  auto context = core::make_ref<gfx::context_t>(true);
  auto base    = core::make_ref<gfx::base_t>(window, context);

  gfx::helper::imgui_init(*window, *context, base->_swapchain,
                          VK_FORMAT_B8G8R8A8_SRGB);

  auto [width, height] = window->dimensions();

  model::raw_model_t raw_model      = model::load_model_from_path(argv[1]);
  raw_model                         = model::merge_meshes(raw_model);
  const model::raw_mesh_t &raw_mesh = raw_model.meshes[0];
  bvh::bvh_t               cpu_bvh  = bvh::build_bvh(raw_mesh);
  bvh_t                    gpu_bvh  = from(cpu_bvh, base);

  gfx::config_pipeline_layout_t cpl{};
  cpl.add_push_constant(sizeof(shader::push_constant_t), VK_SHADER_STAGE_ALL);
  gfx::handle_pipeline_layout_t pl = context->create_pipeline_layout(cpl);

  gfx::config_pipeline_t cp{};
  cp.handle_pipeline_layout = pl;
  cp.add_shader(context->create_shader(gfx::config_shader_t{
      .code_or_path = "./src/shaders/triangles.slang",
      .type         = gfx::shader_type_t::e_vertex,
  }));
  cp.add_shader(context->create_shader(gfx::config_shader_t{
      .code_or_path = "./src/shaders/triangles.slang",
      .type         = gfx::shader_type_t::e_fragment,
  }));
  cp.add_color_attachment(VK_FORMAT_B8G8R8A8_SRGB,
                          gfx::default_color_blend_attachment());

  VkPipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state{};
  vk_pipeline_rasterization_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  vk_pipeline_rasterization_state.depthClampEnable        = VK_FALSE;
  vk_pipeline_rasterization_state.rasterizerDiscardEnable = VK_FALSE;
  vk_pipeline_rasterization_state.polygonMode     = VK_POLYGON_MODE_LINE;
  vk_pipeline_rasterization_state.lineWidth       = 1.0f;
  vk_pipeline_rasterization_state.cullMode        = VK_CULL_MODE_NONE;
  vk_pipeline_rasterization_state.frontFace       = VK_FRONT_FACE_CLOCKWISE;
  vk_pipeline_rasterization_state.depthBiasEnable = VK_FALSE;
  vk_pipeline_rasterization_state.depthBiasConstantFactor = 0.0f;  // Optional
  vk_pipeline_rasterization_state.depthBiasClamp          = 0.0f;  // Optional
  vk_pipeline_rasterization_state.depthBiasSlopeFactor    = 0.0f;  // Optional

  cp.set_pipeline_rasterization_state(vk_pipeline_rasterization_state);
  gfx::handle_pipeline_t p = context->create_graphics_pipeline(cp);

  editor_camera_t              camera{*window};
  gfx::handle_managed_buffer_t view;
  gfx::handle_managed_buffer_t projection;
  gfx::handle_managed_buffer_t inv_view;
  gfx::handle_managed_buffer_t inv_projection;
  {
    gfx::config_buffer_t cb{};
    cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    cb.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    cb.vk_size = sizeof(math::mat4);
    view =
        base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);
    projection =
        base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);
    inv_view =
        base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);
    inv_projection =
        base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);
  }

  core::frame_timer_t frame_timer{60.f};

  core::transform_t transform{};
  transform.scale = {0.1, 0.1, 0.1};
  gfx::handle_buffer_t model;
  {
    gfx::config_buffer_t cb{};
    cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    cb.vma_allocation_create_flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    cb.vk_size = sizeof(math::mat4);
    model      = context->create_buffer(cb);
  }

  while (!window->should_close()) {
    core::window_t::poll_events();
    if (window->get_key_pressed(GLFW_KEY_Q)) break;

    core::timer::duration_t dt = frame_timer.update();
    camera.update(dt.count());

    std::memcpy(context->map_buffer(base->buffer(view)), &camera.view,
                sizeof(math::mat4));
    std::memcpy(context->map_buffer(base->buffer(projection)),
                &camera.projection, sizeof(math::mat4));
    std::memcpy(context->map_buffer(base->buffer(inv_view)), &camera.inv_view,
                sizeof(math::mat4));
    std::memcpy(context->map_buffer(base->buffer(inv_projection)),
                &camera.inv_projection, sizeof(math::mat4));

    auto mat4 = transform.mat4();
    std::memcpy(context->map_buffer(model), &mat4, sizeof(math::mat4));

    base->begin();

    auto cmd = base->current_commandbuffer();

    context->cmd_image_memory_barrier(
        cmd, base->current_swapchain_image(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    gfx::rendering_attachment_t color{};
    color.clear_value       = {0, 0, 0, 0};
    color.handle_image_view = base->current_swapchain_image_view();
    color.image_layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.load_op           = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.store_op          = VK_ATTACHMENT_STORE_OP_STORE;
    VkRect2D rect2d{};
    rect2d.extent = {(uint32_t)width, (uint32_t)height};
    context->cmd_begin_rendering(cmd, {color}, std::nullopt, rect2d);

    auto [viewport, scissor] =
        gfx::helper::fill_viewport_and_scissor_structs(width, height);

    context->cmd_bind_pipeline(cmd, p);
    context->cmd_set_viewport_and_scissor(cmd, viewport, scissor);
    shader::push_constant_t pc{};
    pc.triangles = context->get_buffer_device_address(gpu_bvh.triangles);
    pc.view      = context->get_buffer_device_address(base->buffer(view));
    pc.projection =
        context->get_buffer_device_address(base->buffer(projection));
    pc.inv_view = context->get_buffer_device_address(base->buffer(inv_view));
    pc.inv_projection =
        context->get_buffer_device_address(base->buffer(inv_projection));
    pc.model = context->get_buffer_device_address(model);
    context->cmd_push_constants(cmd, p, VK_SHADER_STAGE_ALL, 0,
                                sizeof(shader::push_constant_t), &pc);
    context->cmd_draw(cmd, cpu_bvh.triangles.size() * 3, 1, 0, 0);

    context->cmd_end_rendering(cmd);

    context->cmd_image_memory_barrier(
        cmd, base->current_swapchain_image(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        {}, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    base->end();
  }

  gfx::helper::imgui_shutdown();

  return 0;
}
