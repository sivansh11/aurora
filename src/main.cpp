#include "camera.hpp"
#include "renderer.hpp"

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "Usage: aurora width height model\n";
    exit(EXIT_FAILURE);
  }

  horizon_info("{}", std::filesystem::current_path().c_str());

  auto renderer =
      core::make_ref<renderer_t>(std::stoi(argv[1]), std::stoi(argv[2]));

  editor_camera_t editor_camera{*renderer->window};

  ecs::scene_t<> scene{};
  {
    auto id = scene.create();
    scene.construct<model::raw_model_t>(id) =
        model::load_model_from_path(argv[3]);
    scene.construct<core::transform_t>(id).scale = {0.1, 0.1, 0.1};
  }

  core::frame_timer_t frame_timer{60.f};

  while (!renderer->window->should_close()) {
    core::window_t::poll_events();
    if (renderer->window->get_key_pressed(GLFW_KEY_Q) ||
        renderer->window->get_key_pressed(GLFW_KEY_ESCAPE))
      break;

    core::timer::duration_t dt            = frame_timer.update();
    editor_camera.camera_speed_multiplyer = renderer->camera_speed;
    editor_camera.update(dt.count());

    renderer->render(scene, editor_camera);
  }

  return 0;
}
