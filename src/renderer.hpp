#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vector>

#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"

struct renderer_t {
  renderer_t(core::ref<core::window_t> window,   //
             core::ref<gfx::context_t> context,  //
             core::ref<gfx::base_t>    base,     //
             const int                 argc,     //
             const char              **argv);
  ~renderer_t();

  void recreate_sized_resources(uint32_t width, uint32_t height);
  std::vector<gfx::pass_t> get_passes();

  core::ref<core::window_t> window;
  core::ref<gfx::context_t> context;
  core::ref<gfx::base_t>    base;

  const int    argc;
  const char **argv;

  uint32_t width = 0, height = 0;

  gfx::handle_image_t      image      = core::null_handle;
  gfx::handle_image_view_t image_view = core::null_handle;
};

#endif
