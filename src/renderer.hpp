#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vector>

#include "horizon/core/components.hpp"
#include "horizon/core/ecs.hpp"
#include "horizon/core/window.hpp"
#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/rendergraph.hpp"
#include "horizon/gfx/types.hpp"
#include "model/model.hpp"

// TODO: add destructor
struct mesh_t {
  gfx::handle_buffer_t vertex_buffer;
  gfx::handle_buffer_t index_buffer;

  uint32_t vertex_count;
  uint32_t index_count;

  gfx::handle_buffer_t transform;

  gfx::handle_image_t      diffuse;
  gfx::handle_image_view_t diffuse_view;

  gfx::handle_bindless_image_t bdiffuse;
};

struct model_t {
  std::vector<mesh_t> meshes;
};

struct diffuse_renderer_t {
  struct push_constant_mesh_t {
    model::vertex_t             *vertices;
    uint32_t                    *indices;
    math::mat4                  *transform;
    gfx::handle_bindless_image_t bdiffuse;
    uint32_t                     padding;
  };
  struct push_constant_t {
    core::camera_t                *camera;
    push_constant_mesh_t           push_constant_mesh;
    gfx::handle_bindless_sampler_t bsampler;
    uint32_t                       padding;
  };

  diffuse_renderer_t(core::ref<core::window_t> window,   //
                     core::ref<gfx::context_t> context,  //
                     core::ref<gfx::base_t>    base,     //
                     VkFormat                  vk_format);
  ~diffuse_renderer_t();

  void render(gfx::handle_commandbuffer_t cbuf, ecs::scene_t<> &scene,
              gfx::handle_buffer_t           camera,
              gfx::handle_bindless_sampler_t bsampler, VkViewport vk_viewport,
              VkRect2D vk_scissor);

  core::ref<core::window_t> window;
  core::ref<gfx::context_t> context;
  core::ref<gfx::base_t>    base;

  gfx::handle_pipeline_layout_t pl;
  gfx::handle_shader_t          v;
  gfx::handle_shader_t          f;
  gfx::handle_pipeline_t        p;
};

struct renderer_t {
  renderer_t(core::ref<core::window_t> window,   //
             core::ref<gfx::context_t> context,  //
             core::ref<gfx::base_t>    base,     //
             const int                 argc,     //
             const char              **argv);
  ~renderer_t();

  void recreate_sized_resources(uint32_t width, uint32_t height);
  std::vector<gfx::pass_t> get_passes(ecs::scene_t<>       &scene,
                                      const core::camera_t &camera);

  core::ref<core::window_t> window;
  core::ref<gfx::context_t> context;
  core::ref<gfx::base_t>    base;

  const int    argc;
  const char **argv;

  uint32_t width = 0, height = 0;

  gfx::handle_sampler_t          sampler;
  gfx::handle_bindless_sampler_t bsampler;

  gfx::handle_descriptor_set_layout_t imgui_dsl;
  gfx::handle_descriptor_set_t        imgui_ds;

  gfx::handle_image_t          white;
  gfx::handle_image_view_t     white_view;
  gfx::handle_bindless_image_t bwhite;

  gfx::handle_image_t      image      = core::null_handle;
  gfx::handle_image_view_t image_view = core::null_handle;
  gfx::handle_image_t      depth      = core::null_handle;
  gfx::handle_image_view_t depth_view = core::null_handle;

  gfx::handle_pipeline_t diffuse;

  gfx::handle_managed_buffer_t camera_buffer;

  core::ref<diffuse_renderer_t> diffuse_renderer;
};

#endif
