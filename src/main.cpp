#include <filesystem>

#include "horizon/core/core.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/logger.hpp"
#include "horizon/core/window.hpp"

#include "renderer.hpp"

#include <GLFW/glfw3.h>

int main(int argc, char **argv) {
  core::log_t::set_log_level(core::log_level_t::e_info);

  check(argc == 4, "aurora [path to assets registry] width height");
  std::filesystem::path assets_path{argv[1]};

  horizon_info("Starting Aurora");
  auto window = core::make_ref<core::window_t>("aurora", std::stoi(argv[2]),
                                               std::stoi(argv[3]));
  aurora::renderer_t renderer{window};

  ecs::scene_t scene;

  while (!window->should_close()) {
    core::window_t::poll_events();
    if (glfwGetKey(*window, GLFW_KEY_ESCAPE))
      break;

    renderer.render(scene);
  }

  return 0;
}
