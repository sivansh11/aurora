#ifndef APP_HPP
#define APP_HPP

#include <vector>

#include "horizon/core/core.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "model/model.hpp"
#include "renderer.hpp"

class app_t {
 public:
  app_t(const int argc, const char **argv);
  ~app_t();
  void run();

 private:
  core::ref<core::window_t> window;
  core::ref<gfx::context_t> context;
  core::ref<gfx::base_t>    base;
  core::ref<renderer_t>     renderer;

  const int    argc;
  const char **argv;
};

#endif
