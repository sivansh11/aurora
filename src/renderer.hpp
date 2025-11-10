#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vector>

#include "horizon/core/ecs.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"

// TODO: add destructor
struct mesh_t {
  gfx::handle_buffer_t vertex_buffer;
  gfx::handle_buffer_t index_buffer;

  uint32_t vertex_count;
  uint32_t index_count;

  gfx::handle_image_t      diffuse;
  gfx::handle_image_view_t diffuse_view;

  gfx::handle_bindless_image_t bdiffuse;
};

struct model_t {
  std::vector<mesh_t> meshes;
};

struct renderer_t {
  renderer_t(core::ref<core::window_t> window,   //
             core::ref<gfx::context_t> context,  //
             core::ref<gfx::base_t>    base,     //
             const int                 argc,     //
             const char              **argv);
  ~renderer_t();

  void recreate_sized_resources(uint32_t width, uint32_t height);
  std::vector<gfx::pass_t> get_passes(ecs::scene_t<> &scene);

  core::ref<core::window_t> window;
  core::ref<gfx::context_t> context;
  core::ref<gfx::base_t>    base;

  const int    argc;
  const char **argv;

  uint32_t width = 0, height = 0;

  gfx::handle_sampler_t          sampler;
  gfx::handle_bindless_sampler_t bsampler;

  gfx::handle_image_t          white;
  gfx::handle_image_view_t     white_view;
  gfx::handle_bindless_image_t bwhite;

  gfx::handle_image_t      image      = core::null_handle;
  gfx::handle_image_view_t image_view = core::null_handle;
};

#endif
