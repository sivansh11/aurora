#include "renderer.hpp"

#include <vulkan/vulkan_core.h>

#include "horizon/core/core.hpp"
#include "horizon/gfx/context.hpp"
#include "horizon/gfx/helper.hpp"

renderer_t::renderer_t(core::ref<core::window_t> window,   //
                       core::ref<gfx::context_t> context,  //
                       core::ref<gfx::base_t>    base,     //
                       const int                 argc,     //
                       const char              **argv)
    : window(window), context(context), base(base), argc(argc), argv(argv) {}

renderer_t::~renderer_t() {}

void renderer_t::recreate_sized_resources(uint32_t width, uint32_t height) {
  if (this->width != width || this->height != height) {
    context->wait_idle();

    this->width  = width;
    this->height = height;

    // destroy sized resources
    if (image != core::null_handle) {
      context->destroy_image(image);
    }
    if (image_view != core::null_handle) {
      context->destroy_image_view(image_view);
    }

    // create sized resources
    gfx::config_image_t ci{};
    ci.vk_width  = width;
    ci.vk_height = height;
    ci.vk_depth  = 1;
    ci.vk_type   = VK_IMAGE_TYPE_2D;
    ci.vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ci.vk_usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |  //
                  VK_IMAGE_USAGE_SAMPLED_BIT |            //
                  VK_IMAGE_USAGE_STORAGE_BIT;
    ci.vma_allocation_create_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    ci.debug_name                  = "image";
    image                          = context->create_image(ci);

    gfx::config_image_view_t civ{.handle_image = image,
                                 .debug_name   = "image view"};
    image_view = context->create_image_view(civ);
  }
}

std::vector<gfx::pass_t> renderer_t::get_passes() { return {}; }
