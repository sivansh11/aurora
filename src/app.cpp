#include "app.hpp"

#include <vulkan/vulkan_core.h>

#include "assets.hpp"
#include "editor_camera.hpp"
#include "horizon/core/components.hpp"
#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"
#include "imgui.h"
#include "math/math.hpp"
#include "model/model.hpp"
#include "renderer.hpp"

app_t::app_t(const int argc, const char** argv) : argc(argc), argv(argv) {
  check(argc == 2, "Usage: [aurora] [model]");
  window     = core::make_ref<core::window_t>("aurora", 640, 420);
  context    = core::make_ref<gfx::context_t>(false /*validations*/);
  base       = core::make_ref<gfx::base_t>(window, context);
  auto_timer = core::make_ref<gpu_auto_timer_t>(base);
  renderer =
      core::make_ref<renderer_t>(window, context, base, auto_timer, argc, argv);

  gfx::helper::imgui_init(
      *window, *context, base->_swapchain,
      context->get_image(context->get_swapchain_images(base->_swapchain)[0])
          .config.vk_format);

  horizon_info("initialised app");
}

app_t::~app_t() {
  context->wait_idle();
  gfx::helper::imgui_shutdown();
  horizon_info("destroyed app");
}

void app_t::run() {
  horizon_info("running app");

  assets_manager_t assets_manager{};
  assets_manager.load_model_from_path(argv[1]);

  auto renderer_data = assets_manager.prepare(base, context, renderer->bwhite);

  uint32_t image_width = 5, image_height = 5;

  core::frame_timer_t frame_timer{60.f};
  float               target_fps = 60.f;
  auto                last_time  = std::chrono::system_clock::now();
  editor_camera_t     camera{*window};
  camera.camera_speed_multiplyer = 100.f;

  while (!window->should_close()) {
    window->poll_events();
    if (window->get_key_pressed(core::key_t::e_q)) break;
    if (window->get_key_pressed(core::key_t::e_escape)) break;

    auto current_time    = std::chrono::system_clock::now();
    auto time_difference = current_time - last_time;
    if (time_difference.count() / 1e6 < 1000.f / target_fps) {
      continue;
    }
    last_time                  = current_time;
    core::timer::duration_t dt = frame_timer.update();

    base->begin();

    gfx::rendergraph_t rg{};
    VkRect2D           vk_rect_2d{};
    auto [width, height]     = window->dimensions();
    vk_rect_2d.extent.width  = width;
    vk_rect_2d.extent.height = height;
    renderer->recreate_sized_resources(image_width, image_height);
    auto renderer_passes = renderer->get_passes(
        renderer_data, reinterpret_cast<core::camera_t&>(camera));
    rg.passes.insert(rg.passes.end(), renderer_passes.begin(),
                     renderer_passes.end());

    static bool clear_auto_timer = false;
    rg.add_pass([&](gfx::handle_commandbuffer_t cmd) {
        gfx::rendering_attachment_t color{};
        color.handle_image_view = base->current_swapchain_image_view();
        color.image_layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.load_op           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.store_op          = VK_ATTACHMENT_STORE_OP_STORE;
        color.clear_value       = {0, 0, 0, 0};
        base->cmd_begin_rendering(cmd, {color}, std::nullopt, vk_rect_2d);
        gfx::helper::imgui_newframe();
        ImGuiDockNodeFlags dockspaceFlags =
            ImGuiDockNodeFlags_None & ~ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |              //
            ImGuiWindowFlags_NoTitleBar |             //
            ImGuiWindowFlags_NoCollapse |             //
            ImGuiWindowFlags_NoResize |               //
            ImGuiWindowFlags_NoMove |                 //
            ImGuiWindowFlags_NoBringToFrontOnFocus |  //
            ImGuiWindowFlags_NoNavFocus |             //
            ImGuiWindowFlags_NoBackground |           //
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
        window_class.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::SetNextWindowClass(&window_class);
        ImGuiWindowFlags viewPortFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
        ImGui::Begin("viewport", nullptr, viewPortFlags);
        // TODO: if should update, set mouse position to center of screen
        camera.update(dt.count(), image_width, image_height);

        // horizon_info("{}", camera.view);
        //
        // ImVec2 mouse_position  = ImGui::GetMousePos();
        // ImVec2 window_position = ImGui::GetWindowPos();
        auto vp = ImGui::GetWindowSize();
        // bool   recreate        = false;
        if (image_width != vp.x || image_height != vp.y) {
          image_width  = vp.x;
          image_height = vp.y;
        }
        ImGui::Image(reinterpret_cast<ImTextureID>(reinterpret_cast<void*>(
                         context->get_descriptor_set(renderer->imgui_ds)
                             .vk_descriptor_set)),
                     ImGui::GetContentRegionAvail());
        // if (ImGui::IsWindowHovered() &&
        //     ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        //     update_debug_trace_ray)
        //   update_debug_trace_ray = false;
        ImGui::End();
        ImGui::PopStyleVar(2);

        if (settings) {
          ImGui::Begin("settings", &settings);
          ImGui::Text("%f fps", ImGui::GetIO().Framerate);
          ImGui::DragFloat("camera speed", &camera.camera_speed_multiplyer);
          const char* rendering_modes[] = {"diffuse", "debug_raytracer",
                                           "raytracer"};
          static int  current_mode      = 0;
          if (ImGui::Combo("Rendering Mode", &current_mode, rendering_modes,
                           IM_ARRAYSIZE(rendering_modes))) {
            switch (current_mode) {
              case 0:
                renderer->rendering_mode =
                    renderer_t::rendering_mode_t::e_diffuse;
                break;
              case 1:
                renderer->rendering_mode =
                    renderer_t::rendering_mode_t::e_debug_raytracer;
                break;
              case 2:
                renderer->rendering_mode =
                    renderer_t::rendering_mode_t::e_raytracer;
                break;
            }
            clear_auto_timer = true;
          }
          for (auto [name, timer] : auto_timer->timers) {
            auto t = context->timer_get_time(base->timer(timer));
            if (t) {
              ImGui::Text("%s took %fms", name.c_str(), *t);
            }
          }
          ImGui::End();
        }
        gfx::helper::imgui_endframe(*context, cmd);
        base->cmd_end_rendering(cmd);
      })
        .add_read_image(renderer->image, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .add_write_image(base->current_swapchain_image(), 0,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rg.add_pass([](gfx::handle_commandbuffer_t) {})
        .add_write_image(base->current_swapchain_image(), 0,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    base->render_rendergraph(rg, base->current_commandbuffer());

    base->end();

    if (clear_auto_timer) {
      clear_auto_timer = false;
      auto_timer->clear();
    }
  }

  context->wait_idle();

  return;
}
