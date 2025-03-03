#ifndef BASIC_PASS_HPP
#define BASIC_PASS_HPP

#include "horizon/core/core.hpp"

#include "horizon/gfx/base.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/types.hpp"

#include <vulkan/vulkan_core.h>

namespace aurora {
struct basic_pass_t {
  struct push_constant_t {
    VkDeviceAddress vertices;
    VkDeviceAddress indices;
    VkDeviceAddress camera;
  };

  basic_pass_t(core::ref<gfx::base_t> base, VkFormat format) : _base(base) {
    gfx::config_shader_t cvs{
      .code_or_path = "../engine-assets/shaders/basic/vert.slang",
      .is_code = false,
      .name = "basic-vertex",
      .type = gfx::shader_type_t::e_vertex,
      .language = gfx::shader_language_t::e_slang,
      .debug_name = "basic-vertex",
    };
    gfx::config_shader_t cfs{
      .code_or_path = "../engine-assets/shaders/basic/frag.slang",
      .is_code = false,
      .name = "basic-fragment",
      .type = gfx::shader_type_t::e_fragment,
      .language = gfx::shader_language_t::e_slang,
      .debug_name = "basic-fragment",
    };
    _vs = _base->_info.context.create_shader(cvs);
    _fs = _base->_info.context.create_shader(cfs);

    gfx::config_descriptor_set_layout_t cdsl{};
    _dsl = _base->_info.context.create_descriptor_set_layout(cdsl);

    gfx::config_pipeline_layout_t cpl{};
    // cpl.add_descriptor_set_layout(cpl);
  }

  core::ref<gfx::base_t> _base;

  gfx::handle_shader_t _vs;
  gfx::handle_shader_t _fs;
  gfx::handle_descriptor_set_layout_t _dsl;

};
} // namespace aurora

#endif
