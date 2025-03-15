#ifndef APP_HPP
#define APP_HPP

#include "horizon/core/core.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

#include <GLFW/glfw3.h>
#include <utility>

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

    gfx::base_config_t base_config{
        .window = *_window,
        .context = *_ctx,
    };
    _base = core::make_ref<gfx::base_t>(base_config);

    gfx::helper::imgui_init(
        *_window, *_ctx, _base->_swapchain,
        _ctx->get_image(_ctx->get_swapchain(_base->_swapchain).handle_images[0])
            .config.vk_format);
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
      auto &swapchain = _ctx->get_swapchain(_base->_swapchain);
      if (_ctx->get_image(swapchain.handle_images[0]).config.vk_width !=
              width ||
          _ctx->get_image(swapchain.handle_images[0]).config.vk_height !=
              height) {
        _base->resize_swapchain();
      }
      // horizon_info("{} {}", width, height);

      _base->begin();

      _base->begin_swapchain_renderpass();

      auto cbuf = _base->current_commandbuffer();

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

      _base->end_swapchain_renderpass();
      _base->end();
    }
  }

private:
  core::ref<core::window_t> _window;
  core::ref<gfx::context_t> _ctx;

  core::ref<gfx::base_t> _base;
};

} // namespace aurora

#endif // !APP_HPP
