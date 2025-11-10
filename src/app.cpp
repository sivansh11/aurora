#include "app.hpp"

#include <vulkan/vulkan_core.h>

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
#include "model/model.hpp"

app_t::app_t(const int argc, const char** argv) : argc(argc), argv(argv) {
  check(argc == 2, "Usage: [aurora] [model]");
  window   = core::make_ref<core::window_t>("aurora", 640, 420);
  context  = core::make_ref<gfx::context_t>(true /*validations*/);
  base     = core::make_ref<gfx::base_t>(window, context);
  renderer = core::make_ref<renderer_t>(window, context, base, argc, argv);

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

  ecs::scene_t<> scene{};
  {
    auto id = scene.create();
    scene.construct<model::raw_model_t>(id) =
        model::load_model_from_path(argv[1]);
  }

  uint32_t image_width = 5, image_height = 5;

  while (!window->should_close()) {
    window->poll_events();
    if (window->get_key_pressed(core::key_t::e_q)) break;
    if (window->get_key_pressed(core::key_t::e_escape)) break;
    base->begin();

    scene.for_all<model::raw_model_t>(
        [&](ecs::entity_id_t id, const model::raw_model_t& raw_model) {
          if (scene.has<model_t>(id)) return;
        });

    gfx::rendergraph_t rg{};
    VkRect2D           vk_rect_2d{};
    auto [width, height]     = window->dimensions();
    vk_rect_2d.extent.width  = width;
    vk_rect_2d.extent.height = height;
    renderer->recreate_sized_resources(image_width, image_height);
    auto renderer_passes = renderer->get_passes();
    rg.passes.insert(rg.passes.end(), renderer_passes.begin(),
                     renderer_passes.end());
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
        // ImVec2 mouse_position  = ImGui::GetMousePos();
        // ImVec2 window_position = ImGui::GetWindowPos();
        // auto   vp              = ImGui::GetWindowSize();
        // bool   recreate        = false;
        // if (image_width != vp.x || image_height != vp.y) {
        //   recreate = true;
        // }
        // ImGui::Image(
        //     reinterpret_cast<ImTextureID>(reinterpret_cast<void*>(
        //         context->get_descriptor_set(imgui_ds).vk_descriptor_set)),
        //     ImGui::GetContentRegionAvail());
        // if (ImGui::IsWindowHovered() &&
        //     ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        //     update_debug_trace_ray)
        //   update_debug_trace_ray = false;
        ImGui::End();
        ImGui::PopStyleVar(2);

        if (settings) {
          ImGui::Begin("settings", &settings);
          ImGui::Text("%f fps", ImGui::GetIO().Framerate);
          ImGui::End();
        }
        gfx::helper::imgui_endframe(*context, cmd);
        base->cmd_end_rendering(cmd);
      })
        .add_write_image(base->current_swapchain_image(), 0,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rg.add_pass([](gfx::handle_commandbuffer_t) {})
        .add_write_image(base->current_swapchain_image(), 0,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    base->render_rendergraph(rg, base->current_commandbuffer());

    base->end();
  }

  return;
}
