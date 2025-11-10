#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include <GLFW/glfw3.h>

#include "horizon/core/components.hpp"
#include "horizon/core/logger.hpp"
#include "imgui.h"

class editor_camera_t : public core::camera_t {
 public:
  editor_camera_t() {}
  ~editor_camera_t() {}

  void update_projection(float aspect) {
    static float s_aspect = 0;
    if (s_aspect != aspect) {
      projection = math::perspective(math::radians(fov), aspect, near, far) *
                   math::scale(math::mat4{1.f}, math::vec3{1.f, -1.f, 1.f});
      s_aspect = aspect;
    }
  }

  void update(float dt) {
    auto vp = ImGui::GetWindowSize();
    update_projection(vp.x / vp.y);

    if (!should_update) ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    if (should_update) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    if (!should_update) return;
    auto mp = ImGui::GetMousePos();

    float velocity = _mouse_speed * dt * camera_speed_multiplyer;

    math::vec3 position = core::camera_t::position();

    if (ImGui::IsKeyPressed(ImGuiKey_W)) position += _front * velocity;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) position -= _front * velocity;
    if (ImGui::IsKeyPressed(ImGuiKey_D)) position += _right * velocity;
    if (ImGui::IsKeyPressed(ImGuiKey_A)) position -= _right * velocity;
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) position += _up * velocity;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftShift)) position -= _up * velocity;

    math::vec2 mouse{mp.x, mp.y};
    math::vec2 difference = mouse - _initial_mouse;
    _initial_mouse        = mouse;

    difference.x = difference.x / float(mp.x);
    difference.y = -(difference.y / float(mp.y));

    _yaw += difference.x * _mouse_sensitivity;
    _pitch += difference.y * _mouse_sensitivity;

    if (_pitch > 89.0f) _pitch = 89.0f;
    if (_pitch < -89.0f) _pitch = -89.0f;

    math::vec3 front;
    front.x = math::cos(math::radians(_yaw)) * math::cos(math::radians(_pitch));
    front.y = math::sin(math::radians(_pitch));
    front.z = math::sin(math::radians(_yaw)) * math::cos(math::radians(_pitch));
    _front  = front;
    _right  = math::normalize(math::cross(_front, math::vec3{0, 1, 0}));
    _up     = math::normalize(math::cross(_right, _front));

    view = math::lookAt(position, position + _front, math::vec3{0, 1, 0});

    core::camera_t::update();
  }

  float fov{45.0f};
  float camera_speed_multiplyer{1.0f};
  float far{1000.0f};
  float near{0.1f};
  bool  should_update = false;

 private:
  math::vec3 _front{0.0f};
  math::vec3 _up{0.0f, 1.0f, 0.0f};
  math::vec3 _right{0.0f};

  math::vec2 _initial_mouse{};

  float _yaw{0.0f};
  float _pitch{0.0f};
  float _mouse_speed{0.005f};
  float _mouse_sensitivity{100.0f};
};

#endif
