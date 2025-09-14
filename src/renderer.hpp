#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <cstddef>
#include <cstring>
#include <optional>

#include "bvh/bvh.hpp"
#include "horizon/core/components.hpp"
#include "horizon/core/logger.hpp"
#include "imgui.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include "horizon/core/ecs.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/types.hpp"
#include "math/math.hpp"
#include "model/model.hpp"

struct mesh_t {
  gfx::handle_buffer_t vertices;
  gfx::handle_buffer_t raw_indices;
  uint32_t             vertex_count;
  uint32_t             raw_index_count;

  gfx::handle_buffer_t nodes;
  gfx::handle_buffer_t bvh_triangles;
  gfx::handle_buffer_t bvh_indices;
  uint32_t node_count;  // triangles and bvh_index count is raw_index_count / 3

  gfx::handle_buffer_t transform;
};

struct model_t {
  std::vector<mesh_t> meshes;
};

namespace shader {

struct push_constant_t {
  VkDeviceAddress camera;
  VkDeviceAddress transform;
  VkDeviceAddress bvh_triangles;
  VkDeviceAddress vertices;
  VkDeviceAddress raw_indices;
};
static_assert(sizeof(push_constant_t) <= 128,
              "sizeof(push_constant_t) should be less than= 128");

}  // namespace shader

struct renderer_t {
  renderer_t(uint32_t width, uint32_t height) {
    window  = core::make_ref<core::window_t>("aurora", width, height);
    context = core::make_ref<gfx::context_t>(true);
    base    = core::make_ref<gfx::base_t>(window, context);

    gfx::helper::imgui_init(
        *window, *context, base->_swapchain,
        context
            ->get_image(
                context->get_swapchain(base->_swapchain).handle_images[0])
            .config.vk_format);

    {
      gfx::config_buffer_t cb{};
      cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      cb.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
      cb.vk_size = sizeof(core::camera_t);
      camera =
          base->create_buffer(gfx::resource_update_policy_t::e_every_frame, cb);
    }

    gfx::config_image_t ci{};
    ci.vk_width                    = width;
    ci.vk_height                   = height;
    ci.vk_depth                    = 1;
    ci.vk_mips                     = 1;
    ci.vk_type                     = VK_IMAGE_TYPE_2D;
    ci.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    ci.vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ci.vk_usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ci.debug_name = "image";
    image         = context->create_image(ci);
    image_view    = context->create_image_view({.handle_image = image});

    ci.debug_name = "depth";
    ci.vk_format  = VK_FORMAT_D32_SFLOAT;
    ci.vk_usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth         = context->create_image(ci);
    depth_view    = context->create_image_view({.handle_image = depth});

    image_width  = width;
    image_height = height;

    {
      imgui_sampler = context->create_sampler({});
      gfx::config_descriptor_set_layout_t cdsl{};
      cdsl.add_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                              VK_SHADER_STAGE_FRAGMENT_BIT);
      cdsl.use_bindless = false;
      imgui_dsl         = context->create_descriptor_set_layout(cdsl);
      imgui_ds          = context->allocate_descriptor_set(
          {.handle_descriptor_set_layout = imgui_dsl});
      context->update_descriptor_set(imgui_ds)
          .push_image_write(
              0,
              gfx::image_descriptor_info_t{
                  .handle_sampler    = imgui_sampler,
                  .handle_image_view = image_view,
                  .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
          .commit();
    }

    gfx::config_pipeline_layout_t cpl{};
    cpl.add_push_constant(sizeof(shader::push_constant_t), VK_SHADER_STAGE_ALL);
    pl = context->create_pipeline_layout(cpl);

    {
      gfx::config_pipeline_t cp{};
      cp.handle_pipeline_layout = pl;
      cp.add_color_attachment(context->get_image(image).config.vk_format,
                              gfx::default_color_blend_attachment());
      cp.set_depth_attachment(
          VK_FORMAT_D32_SFLOAT,
          VkPipelineDepthStencilStateCreateInfo{
              .sType =
                  VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
              .depthTestEnable   = VK_TRUE,
              .depthWriteEnable  = VK_TRUE,
              .depthCompareOp    = VK_COMPARE_OP_LESS,
              .stencilTestEnable = VK_FALSE,
          });
      debug_draw_vertex = gfx::helper::create_slang_shader(
          *context, "./src/shaders/debug_draw.slang",
          gfx::shader_type_t::e_vertex);
      cp.add_shader(debug_draw_vertex);
      debug_draw_fragment = gfx::helper::create_slang_shader(
          *context, "./src/shaders/debug_draw.slang",
          gfx::shader_type_t::e_fragment);
      cp.add_shader(debug_draw_fragment);
      VkPipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state{};
      vk_pipeline_rasterization_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      vk_pipeline_rasterization_state.depthClampEnable        = VK_FALSE;
      vk_pipeline_rasterization_state.rasterizerDiscardEnable = VK_FALSE;
      vk_pipeline_rasterization_state.polygonMode     = VK_POLYGON_MODE_FILL;
      vk_pipeline_rasterization_state.lineWidth       = 1.0f;
      vk_pipeline_rasterization_state.cullMode        = VK_CULL_MODE_NONE;
      vk_pipeline_rasterization_state.frontFace       = VK_FRONT_FACE_CLOCKWISE;
      vk_pipeline_rasterization_state.depthBiasEnable = VK_FALSE;
      vk_pipeline_rasterization_state.depthBiasConstantFactor =
          0.0f;                                                     // Optional
      vk_pipeline_rasterization_state.depthBiasClamp       = 0.0f;  // Optional
      vk_pipeline_rasterization_state.depthBiasSlopeFactor = 0.0f;  // Optional
      cp.set_pipeline_rasterization_state(vk_pipeline_rasterization_state);
      debug_draw_pipeline = context->create_graphics_pipeline(cp);
    }
  }
  ~renderer_t() {
    context->wait_idle();
    context->destroy_pipeline(debug_draw_pipeline);
    context->destroy_shader(debug_draw_fragment);
    context->destroy_shader(debug_draw_vertex);
    context->destroy_pipeline_layout(pl);
    context->destroy_image_view(depth_view);
    context->destroy_image(depth);
    gfx::helper::imgui_shutdown();
  }

  void render(ecs::scene_t<>& scene, const core::camera_t& camera) {
    scene.for_all<model::raw_model_t, core::transform_t>(
        [&](ecs::entity_id_t id, model::raw_model_t& raw_model,
            const core::transform_t& transform) {
          if (scene.has<model_t>(id)) return;
          // // NOTE: only for now
          raw_model      = model::merge_meshes(raw_model);
          model_t& model = scene.construct<model_t>(id);
          for (auto raw_mesh : raw_model.meshes) {
            bvh::bvh_t bvh       = bvh::build_bvh(raw_mesh);
            mesh_t&    mesh      = model.meshes.emplace_back();
            mesh.vertex_count    = raw_mesh.vertices.size();
            mesh.raw_index_count = raw_mesh.indices.size();
            mesh.node_count      = bvh.nodes.size();

            gfx::config_buffer_t cb{};
            cb.vk_buffer_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            cb.vma_allocation_create_flags =
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

            cb.vk_size =
                raw_mesh.vertices.size() * sizeof(raw_mesh.vertices[0]);
            mesh.vertices = gfx::helper::create_buffer_staged(
                *context, base->_command_pool, cb, raw_mesh.vertices.data(),
                cb.vk_size);
            cb.vk_size = raw_mesh.indices.size() * sizeof(raw_mesh.indices[0]);
            mesh.raw_indices = gfx::helper::create_buffer_staged(
                *context, base->_command_pool, cb, raw_mesh.indices.data(),
                cb.vk_size);
            cb.vk_size = bvh.nodes.size() * sizeof(bvh.nodes[0]);
            mesh.nodes = gfx::helper::create_buffer_staged(
                *context, base->_command_pool, cb, bvh.nodes.data(),
                cb.vk_size);
            cb.vk_size = bvh.triangles.size() * sizeof(bvh.triangles[0]);
            mesh.bvh_triangles = gfx::helper::create_buffer_staged(
                *context, base->_command_pool, cb, bvh.triangles.data(),
                cb.vk_size);
            cb.vk_size       = bvh.indices.size() * sizeof(bvh.indices[0]);
            mesh.bvh_indices = gfx::helper::create_buffer_staged(
                *context, base->_command_pool, cb, bvh.indices.data(),
                cb.vk_size);

            cb.vma_allocation_create_flags =
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            cb.vk_size     = sizeof(math::mat4);
            mesh.transform = context->create_buffer(cb);

            math::mat4 mat4 = transform.mat4();
            std::memcpy(context->map_buffer(mesh.transform), &mat4,
                        sizeof(mat4));
          }
        });

    std::memcpy(context->map_buffer(base->buffer(this->camera)), &camera,
                sizeof(camera));

    base->begin();

    auto cmd = base->current_commandbuffer();

    auto [width, height] = window->dimensions();
    auto [viewport, scissor] =
        gfx::helper::fill_viewport_and_scissor_structs(width, height);

    context->cmd_image_memory_barrier(
        cmd, image,
        VK_IMAGE_LAYOUT_UNDEFINED,                       //
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        //
        0,                                               //
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,            //
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,               //
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);  //

    context->cmd_image_memory_barrier(
        cmd, depth,
        VK_IMAGE_LAYOUT_UNDEFINED,                         //
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  //
        0,                                                 //
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,      //
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,                 //
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |       //
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);    //

    {
      gfx::rendering_attachment_t rendering_attachment{};
      rendering_attachment.handle_image_view = image_view;
      rendering_attachment.image_layout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      rendering_attachment.load_op     = VK_ATTACHMENT_LOAD_OP_CLEAR;
      rendering_attachment.store_op    = VK_ATTACHMENT_STORE_OP_STORE;
      rendering_attachment.clear_value = {0, 0, 0, 0};

      gfx::rendering_attachment_t rendering_attachment_depth{};
      rendering_attachment_depth.handle_image_view = depth_view;
      rendering_attachment_depth.image_layout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      rendering_attachment_depth.load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
      rendering_attachment_depth.store_op = VK_ATTACHMENT_STORE_OP_STORE;
      rendering_attachment_depth.clear_value.depthStencil.depth = 1;

      VkRect2D vk_rect2d{};
      vk_rect2d.extent.width  = image_width;
      vk_rect2d.extent.height = image_height;

      context->cmd_begin_rendering(cmd, {rendering_attachment},
                                   rendering_attachment_depth, vk_rect2d);
    }
    context->cmd_bind_pipeline(cmd, debug_draw_pipeline);
    context->cmd_set_viewport_and_scissor(cmd, viewport, scissor);
    scene.for_all<model_t>([&](ecs::entity_id_t id, const model_t& model) {
      for (auto mesh : model.meshes) {
        shader::push_constant_t pc{};
        pc.camera =
            context->get_buffer_device_address(base->buffer(this->camera));
        pc.transform = context->get_buffer_device_address(mesh.transform);
        pc.bvh_triangles =
            context->get_buffer_device_address(mesh.bvh_triangles);
        pc.vertices    = context->get_buffer_device_address(mesh.vertices);
        pc.raw_indices = context->get_buffer_device_address(mesh.raw_indices);
        context->cmd_push_constants(cmd, debug_draw_pipeline,
                                    VK_SHADER_STAGE_ALL, 0, sizeof(pc), &pc);
        context->cmd_draw(cmd, mesh.raw_index_count, 1, 0, 0);
      }
    });
    context->cmd_end_rendering(cmd);

    context->cmd_image_memory_barrier(
        cmd, image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,       //
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,       //
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,           //
        VK_ACCESS_SHADER_READ_BIT,                      //
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  //
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);         //

    context->cmd_image_memory_barrier(
        cmd, base->current_swapchain_image(),
        VK_IMAGE_LAYOUT_UNDEFINED,                       //
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        //
        {},                                              //
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,            //
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,               //
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);  //

    {
      gfx::rendering_attachment_t rendering_attachment{};
      rendering_attachment.handle_image_view =
          base->current_swapchain_image_view();
      rendering_attachment.image_layout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      rendering_attachment.load_op     = VK_ATTACHMENT_LOAD_OP_CLEAR;
      rendering_attachment.store_op    = VK_ATTACHMENT_STORE_OP_STORE;
      rendering_attachment.clear_value = {0, 0, 0, 0};

      VkRect2D vk_rect2d{};
      vk_rect2d.extent.width  = width;
      vk_rect2d.extent.height = height;

      context->cmd_begin_rendering(cmd, {rendering_attachment}, std::nullopt,
                                   vk_rect2d);
    }

    gfx::helper::imgui_newframe();

    ImGuiDockNodeFlags dockspaceFlags =
        ImGuiDockNodeFlags_None & ~ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoDecoration;

    bool dockSpace = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    auto mainViewPort = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewPort->WorkPos);
    ImGui::SetNextWindowSize(mainViewPort->WorkSize);
    ImGui::SetNextWindowViewport(mainViewPort->ID);

    ImGui::Begin("DockSpace", &dockSpace, windowFlags);
    ImGuiID dockspaceID = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspaceID, ImGui::GetContentRegionAvail(),
                     dockspaceFlags);
    static bool settings = true;
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("menu")) {
        if (ImGui::MenuItem("show settings", NULL, &settings)) {
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGuiWindowFlags viewPortFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
    ImGui::Begin("viewport", nullptr, viewPortFlags);
    auto vp       = ImGui::GetWindowSize();
    bool recreate = false;
    if (image_width != vp.x || image_height != vp.y) {
      recreate = true;
    }
    ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void*>(
                     context->get_descriptor_set(imgui_ds).vk_descriptor_set)),
                 ImGui::GetContentRegionAvail());
    ImGui::End();
    ImGui::PopStyleVar(2);

    if (settings) {
      ImGui::Begin("settings", &settings);
      ImGui::End();
    }

    gfx::helper::imgui_endframe(*context, cmd);

    context->cmd_end_rendering(cmd);

    context->cmd_image_memory_barrier(
        cmd, base->current_swapchain_image(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,       //
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                //
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,           //
        {},                                             //
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  //
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);          //

    base->end();

    if (recreate) {
      context->wait_idle();

      context->destroy_image(image);
      context->destroy_image(depth);
      context->destroy_image_view(image_view);
      context->destroy_image_view(depth_view);

      gfx::config_image_t ci{};
      ci.vk_width  = vp.x;
      ci.vk_height = vp.y;
      ci.vk_depth  = 1;
      ci.vk_mips   = 1;
      ci.vk_type   = VK_IMAGE_TYPE_2D;
      ci.vma_allocation_create_flags =
          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

      ci.vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
      ci.vk_usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
      ci.debug_name = "image";
      image         = context->create_image(ci);
      image_view    = context->create_image_view({.handle_image = image});

      ci.vk_format  = VK_FORMAT_D32_SFLOAT;
      ci.vk_usage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      ci.debug_name = "depth";
      depth         = context->create_image(ci);
      depth_view    = context->create_image_view({.handle_image = depth});

      image_width  = vp.x;
      image_height = vp.y;

      context->update_descriptor_set(imgui_ds)
          .push_image_write(
              0,
              gfx::image_descriptor_info_t{
                  .handle_sampler    = imgui_sampler,
                  .handle_image_view = image_view,
                  .vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
          .commit();
    }
  }

  core::ref<core::window_t> window;
  core::ref<gfx::context_t> context;
  core::ref<gfx::base_t>    base;

  gfx::handle_managed_buffer_t camera;

  gfx::handle_sampler_t               imgui_sampler;
  gfx::handle_descriptor_set_layout_t imgui_dsl;
  gfx::handle_descriptor_set_t        imgui_ds;

  gfx::handle_image_t      image;
  gfx::handle_image_view_t image_view;
  gfx::handle_image_t      depth;
  gfx::handle_image_view_t depth_view;

  uint32_t image_width, image_height;

  gfx::handle_pipeline_layout_t pl;

  gfx::handle_shader_t debug_draw_vertex;
  gfx::handle_shader_t debug_draw_fragment;

  gfx::handle_pipeline_t debug_draw_pipeline;
};

#endif
