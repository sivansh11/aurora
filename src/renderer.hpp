#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "horizon/core/core.hpp"

#include "horizon/core/ecs.hpp"
#include "horizon/core/window.hpp"

#include "horizon/gfx/context.hpp"

namespace aurora {

class renderer_t {
public:
  renderer_t(core::ref<core::window_t> window) : _window(window) {
#ifdef AURORA_NO_GFX_VALIDATION
    _ctx = core::make_ref<gfx::context_t>(false);
#else
    _ctx = core::make_ref<gfx::context_t>(true);
#endif
  }

  ~renderer_t() {}

  template <size_t page_size>
  void render(ecs::scene_t<page_size>& scene) {

  }

private:
  core::ref<core::window_t> _window;
  core::ref<gfx::context_t> _ctx;
};

} // namespace aurora

#endif
