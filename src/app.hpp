#ifndef APP_HPP
#define APP_HPP

#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

#include <GLFW/glfw3.h>

namespace aurora {

class app_t {
public:
  app_t(int argc, char **argv) {
    _window = core::make_ref<core::window_t>("aurora", 1200, 900);
#ifdef AURORA_NO_GFX_VALIDATION
    _ctx = core::make_ref<gfx::context_t>(false);
#else
    _ctx = core::make_ref<gfx::context_t>(true);
#endif // AURORA_NO_GFX_VALIDATION

    auto [width, height] = _window->dimensions();

    _global_sampler = _ctx->create_sampler({});

    gfx::config_image_t config_final_image{};
    config_final_image.vk_width = width;
    config_final_image.vk_height = height;
    config_final_image.vk_depth = 1;
    config_final_image.vk_type = VK_IMAGE_TYPE_2D;
    config_final_image.vk_format = _final_image_format;
    config_final_image.vk_usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    config_final_image.vk_mips = 1;
    config_final_image.debug_name = "final image";
    _final_image = _ctx->create_image(config_final_image);
    _final_image_view = _ctx->create_image_view({.handle_image = _final_image});

    gfx::base_config_t base_config{.window = *_window,
                                   .context = *_ctx,
                                   .sampler = _global_sampler,
                                   .final_image_view = _final_image_view};
    _base = core::make_ref<gfx::base_t>(base_config);

    gfx::helper::imgui_init(*_window, *_ctx, _base->_swapchain,
                            _final_image_format);
  }

  ~app_t() {
    _ctx->wait_idle();
    gfx::helper::imgui_shutdown();
  }

  void run() {
    while (!_window->should_close()) {
      core::window_t::poll_events();
      if (glfwGetKey(*_window, GLFW_KEY_ESCAPE))
        break;

      auto [width, height] = _window->dimensions();
      _base->begin();

      auto cbuf = _base->current_commandbuffer();

      _ctx->cmd_image_memory_barrier(
          cbuf, _final_image, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

      gfx::rendering_attachment_t color_rendering_attachment{};
      color_rendering_attachment.clear_value = {0, 0, 0, 0};
      color_rendering_attachment.image_layout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      color_rendering_attachment.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
      color_rendering_attachment.store_op = VK_ATTACHMENT_STORE_OP_STORE;
      color_rendering_attachment.handle_image_view = _final_image_view;

      _ctx->cmd_begin_rendering(cbuf, {color_rendering_attachment},
                                std::nullopt,
                                VkRect2D{VkOffset2D{},
                                         {static_cast<uint32_t>(width),
                                          static_cast<uint32_t>(height)}});

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
      if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("options")) {
          // for (auto &panel : pipelines) {
          //   if (ImGui::MenuItem(panel->m_name.c_str(), NULL, &panel->show)) {
          //   }
          // }
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
      // ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void *>(
      //                  _ctx->get_descriptor_set(
      //                          chip8_descriptor_set[_base->current_frame()])
      //                      .vk_descriptor_set)),
      //              ImGui::GetContentRegionAvail(), ImVec2(0, -1), ImVec2(1,
      //              0));
      ImGui::End();
      ImGui::PopStyleVar(2);

      ImGui::Begin("test");
      ImGui::End();

      gfx::helper::imgui_endframe(*_ctx, cbuf);

      _ctx->cmd_end_rendering(cbuf);

      // transition image to shader read only optimal
      _ctx->cmd_image_memory_barrier(
          cbuf, _final_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

      _base->end();
    }
  }

private:
  core::ref<core::window_t> _window;
  core::ref<gfx::context_t> _ctx;

  VkFormat _final_image_format = VK_FORMAT_R8G8B8A8_UNORM;
  gfx::handle_sampler_t _global_sampler;
  gfx::handle_image_t _final_image;
  gfx::handle_image_view_t _final_image_view;

  core::ref<gfx::base_t> _base;
};

} // namespace aurora

#endif // !APP_HPP
